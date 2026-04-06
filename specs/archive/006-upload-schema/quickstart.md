# Quickstart: Tenant Admin Schema Upload

**Feature**: `006-upload-schema`
**Audience**: Developers validating tenant-scoped schema document upload/list/fetch behavior

## Goal

Validate authorization, overwrite conflict semantics, case-sensitive naming, configurable size-limit behavior (default `1 MB`), overwrite concurrency resolution, list metadata contract, tenant isolation, and durable persistence.

## Prerequisites

- Build configured (for example via `python3 configure.py`)
- Test binaries built in `cmake-build-debug/`
- Running backend instance optional for manual GraphQL probing

## Validation matrix

| Requirement | Validation surface |
| --- | --- |
| FR-001, FR-004, FR-005, SC-001 | Integration tests for tenant_admin vs non-admin/unauthenticated upload attempts |
| FR-002, SC-002 | Integration/unit tests for collision rejection without overwrite and replacement with overwrite |
| FR-003, FR-010, FR-012, SC-003 | Upload/fetch tests with invalid SDL, empty content, invalid names, and case-sensitive name matching |
| FR-006, FR-008, FR-009, FR-015, SC-005, SC-006, SC-010 | List/fetch tests for authenticated organization members, canonical null-on-miss fetch behavior, and exact list metadata fields |
| FR-007, SC-004 | Cross-tenant isolation tests for list/fetch and duplicate names in different tenants |
| FR-013, SC-008 | Size-limit tests above/at limit and default-limit behavior when not configured |
| FR-014, SC-009 | Concurrency tests for overwrite races where last successful commit wins atomically |
| FR-011 | Restart persistence test proving uploaded schema remains retrievable after controlled backend restart |
| SC-007 | Backend integration performance test proving >=95% of valid uploads complete with definitive outcome in under 2 seconds |

## 1) Run focused backend tests

```bash
cd /home/groby/dev/isched/cmake-build-debug
ctest -R "isched_gql_executor_tests|test_graphql_schema_upload" --output-on-failure
```

## 2) Run full backend regression gate

```bash
cd /home/groby/dev/isched/cmake-build-debug
ctest --output-on-failure
```

## 3) Optional manual GraphQL probe (authenticated tenant_admin)

```bash
curl -sS http://localhost:8080/graphql \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer <TOKEN>' \
  -d '{"query":"mutation($input: UploadSchemaDocumentInput!){ uploadSchemaDocument(input:$input){ success error{code message conflictingName} schema{name updatedAt}} }","variables":{"input":{"name":"billing-v1","content":"type Query { hello: String }","overwrite":false}}}'
```

## 4) Optional manual list/fetch probes (authenticated organization member)

```bash
curl -sS http://localhost:8080/graphql \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer <TOKEN>' \
  -d '{"query":"query { schemaDocuments { name createdAt updatedAt updatedBy } }"}'

curl -sS http://localhost:8080/graphql \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer <TOKEN>' \
  -d '{"query":"query($name:String!){ schemaDocument(name:$name){ name content } }","variables":{"name":"billing-v1"}}'
```

## Troubleshooting

- If uploads fail as unauthenticated unexpectedly, verify JWT/session validity and resolver auth context.
- If overwrite behavior is inconsistent, verify conflict path checks before DB write and atomic update path when `overwrite=true`.
- If SDL rejections look too generic, verify parser error mapping keeps `VALIDATION_FAILED` with descriptive message.
- If cross-tenant leakage appears, verify tenant ID is sourced from authenticated resolver context only.

---

## T039: Implementation Behavior Notes

### Name Validation Rules
- Regex: `[A-Za-z0-9._-]{1,128}` (dots, dashes, underscores, alphanumeric only)
- Case-sensitive: `Billing` and `billing` are distinct documents
- Empty name or name > 128 chars → `VALIDATION_FAILED`
- Names with spaces, slashes, or other special chars → `VALIDATION_FAILED`

### Content Validation Rules
- Empty content → `VALIDATION_FAILED`
- Content > 1 MB (1,048,576 bytes) → `VALIDATION_FAILED` (early rejection before SDL parse)
- Content not parseable as valid GraphQL SDL by PEGTL grammar → `VALIDATION_FAILED`

### Conflict / Overwrite Semantics
- Default `overwrite=false`: duplicate name → `CONFLICT` with `conflictingName` populated
- Explicit `overwrite=true`: atomic `BEGIN IMMEDIATE` transaction replaces content + updates `updated_at`/`updated_by`
- Concurrent overwrites: last successful SQLite commit wins; no partial writes possible

### Null-on-Miss Fetch Behavior
- `schemaDocument(name: "missing")` → `{ "data": { "schemaDocument": null } }` with no `errors`
- Case-sensitive lookup: `MYSDL` does not resolve `mysdl`

### Cross-Tenant Isolation
- All DB queries scoped to authenticated `ResolverCtx.tenant_id`
- No `organizationId` argument; no cross-tenant lookup is possible by design

### Restart Durability
- `schema_documents` table persists in per-tenant SQLite file
- Content survives executor + `DatabaseManager` destruction (in-process restart simulation)

---

## T040: Regression and Performance Verification Evidence

### Test run: 2026-04-06

#### Focused gate
```
ctest -R "isched_gql_executor_tests|test_graphql_schema_upload" --output-on-failure
```
Result: **PASS** — 37 test cases (isched_gql_executor_tests) + 13 test cases / 1561 assertions (test_graphql_schema_upload)

#### Full gate
```
ctest --output-on-failure -j1
100% tests passed, 0 tests failed out of 40
Total Test time (real) = 122.61 sec
```

#### Performance thresholds (observed)
| Test | Threshold | Observed P95 |
|------|-----------|--------------|
| SC-007: Upload P95 (N=100) | < 2000 ms | << 2000 ms ✓ |
| SC-010: List P95 (N=100, 200 docs) | < 500 ms | << 500 ms ✓ |
| SC-006: Fetch P95 (N=100, 200 docs) | < 300 ms | << 300 ms ✓ |
