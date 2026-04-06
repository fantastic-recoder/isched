## [Feature 006] Tenant Admin Schema Upload / List / Fetch — 2026-04-06

### Added
- `uploadSchemaDocument(input: UploadSchemaDocumentInput!): SchemaUploadResult!` mutation — allows authenticated `tenant_admin` users to upload or replace a named GraphQL SDL schema document within their tenant scope.
  - Accepts `name` (validated against `[A-Za-z0-9._-]{1,128}`), `content` (max 1 MB, PEGTL-parsed SDL), and `overwrite` flag (default `false`).
  - Returns structured `SchemaUploadResult` with `success`, `schema { name createdAt updatedAt updatedBy }`, and `error { code message conflictingName }`.
  - Duplicate uploads without `overwrite=true` return `CONFLICT` with `conflictingName`.
  - Non-admin and unauthenticated callers receive `FORBIDDEN` (RBAC gate enforced before resolver entry).
- `schemaDocuments: [SchemaDocumentSummary!]!` query — lists all schema document summaries for the authenticated tenant (excludes `content` and `sizeBytes`).
- `schemaDocument(name: String!): SchemaDocument` query — fetches full SDL content for a single document by exact name; returns `null` without errors when name is absent (canonical null-on-miss).
- `schema_documents` SQLite table in each tenant DB: columns `name` (PK), `content`, `content_sha256`, `created_at`, `updated_at`, `updated_by`; provisioned automatically during `initialize_tenant()`.
- New DB API methods: `ensure_schema_documents_table`, `insert_schema_document`, `replace_schema_document` (atomic IMMEDIATE transaction), `list_schema_documents`, `get_schema_document_by_name`.
- `CryptoUtils::sha256_hex()` helper for content digest computation.
- New GraphQL SDL types: `UploadSchemaDocumentInput`, `SchemaDocumentSummary`, `SchemaDocument`, `SchemaUploadError`, `SchemaUploadResult`.
- Integration test suite `test_graphql_schema_upload` covering SC-001 through SC-010:
  - Authorization (tenant_admin only; unauthenticated + plain-user rejection)
  - Conflict/overwrite semantics; case-sensitive name uniqueness
  - Validation (name regex, size limit, empty/malformed SDL)
  - Cross-tenant isolation for list and fetch
  - List metadata contract (exact fields, no `sizeBytes`)
  - Controlled-restart durability (in-process recreate)
  - Concurrent overwrite resolves to last successful commit
  - Upload P95 < 2 s (N=100), list P95 < 500 ms (200 docs), fetch P95 < 300 ms (200 docs)
- Feature threat model in `specs/006-upload-schema/threat-model.md` covering 7 identified threats.
- Feature 006 security closeout snapshot in `docs/security-threat-model.md`.

### Changed
- `DatabaseManager::initialize_tenant()` now provisions the `schema_documents` table automatically.

### Validation
- Backend focused gate: `ctest -R test_graphql_schema_upload --output-on-failure` -> PASS (13 test cases, 1561 assertions).
- Backend full gate: `ctest --output-on-failure` -> PASS (40/40).

## [Feature 005] RATE_LIMITED + Auth Bootstrap Consistency (Phase 6) — 2026-04-05

### Added
- Final mitigation/evidence closeout for feature threat modeling in `specs/005-rate-limited-auth-bootstrap/threat-model.md`.
- Project-level security summary snapshot for Feature 005 in `docs/security-threat-model.md`.
- Recorded full feature regression command matrix with observed outcomes in `specs/005-rate-limited-auth-bootstrap/quickstart.md`.

### Validation
- Backend focused gate: `ctest -R 'test_rate_limiting|test_seed_mode|isched_auth_tests' --output-on-failure` -> PASS.
- Backend full gate: `ctest --output-on-failure` -> PASS (39/39).
- Frontend focused unit gates: `pnpm run test:login-lockout`, `pnpm run test:startup-routing`, `pnpm run test:auth-bootstrap` -> PASS.
- Frontend full unit gate: `pnpm test` -> PASS (16 suites / 86 tests).
- Playwright focused gate: `pnpm run e2e:bootstrap` -> PASS (3/3).
- Playwright lockout/full gates: `pnpm run e2e:rate-limiting`, `pnpm run e2e:auth-bootstrap`, `pnpm e2e` -> PASS (4/4, 7/7, 7/7).

### Blockers
- Previously reported lockout E2E selector/classification mismatch in `src/ui/e2e/rate-limiting.spec.ts` is resolved.

### Status
- Phase 6 tasks T035-T038 remain complete, and Feature 005 E2E closeout gates are green after lockout assertion alignment.

## [Feature 001] Universal Backend Closeout — 2026-04-04

### Added
- Explicit closeout evidence indexing for additional success criteria:
    - **SC-001** timed quickstart validation
    - **SC-004** configuration activation latency validation
- Concrete traceability links in `specs/001-universal-backend/closeout-validation.md` to:
    - `specs/001-universal-backend/quickstart.md`
    - `src/test/cpp/integration/test_server_startup.cpp`
    - `src/test/cpp/integration/test_schema_activation.cpp`
    - `specs/001-universal-backend/plan.md`

### Changed
- Finalized closeout wording and traceability across:
    - `specs/001-universal-backend/spec.md`
    - `specs/001-universal-backend/plan.md`
    - `specs/001-universal-backend/tasks.md`
    - `specs/001-universal-backend/closeout-validation.md`
- Normalized remaining terminology and documentation consistency issues for sign-off quality.
- Confirmed SC-005 capability checklist outcome:
    - **Threshold:** >=19/20
    - **Measured:** **20/20**
    - **Decision:** **PASS**

### Validation
- Full suite gate executed via:
    - `ctest --output-on-failure`
- Final analyze status:
    - **CRITICAL: 0**
    - **HIGH: 0**
    - **MEDIUM: 0**
    - residual LOW items only (non-blocking editorial debt)

### Closeout Commits
- `ae234b9` — `fix(spec): polish feature 001-universal-backend closeout wording`
- `7365b21` — `fix(specs): tighten feature 001 closeout traceability`
- `fe3237d` — `fix(specs): finalize feature 001 documentation cleanup`
- `9303ee0` — `fix(specs): finalize feature 001 closeout marker` (empty marker commit)

### Status
- **Feature `001-universal-backend` is closeout-ready and sign-off complete from artifact-consistency and evidence-traceability perspectives.**
