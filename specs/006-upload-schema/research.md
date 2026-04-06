# Research: Tenant Admin Schema Upload

**Phase**: 0 (Outline & Research)  
**Updated**: 2026-04-06  
**Feature**: `006-upload-schema`

## Decision 1: Keep the external contract GraphQL-only with dedicated schema-document operations

- **Decision**: Introduce explicit GraphQL operations for upload/list/fetch of schema documents and do not introduce REST or filesystem endpoints.
- **Rationale**: Constitution requires GraphQL-only external interfaces and the feature requirements are already expressed as GraphQL behaviors.
- **Alternatives considered**:
  - Add REST upload/list routes: rejected due to constitutional violation.
  - Reuse configuration snapshot mutations for this use case: rejected because snapshots are versioned configuration artifacts, not organization-visible schema document registry records.

## Decision 2: Use tenant-local SQLite persistence with name uniqueness enforced in-table

- **Decision**: Persist documents in each tenant DB (`schema_documents`) with `name` as organization-unique key per tenant database.
- **Rationale**: Existing architecture already enforces hard tenant isolation with per-tenant SQLite files; per-tenant table keying gives uniqueness without cross-tenant coupling.
- **Alternatives considered**:
  - Store in `isched_system.db` with `organization_id + name` composite key: rejected to avoid broadening cross-tenant blast radius.
  - Store only in-memory: rejected because FR-011 requires durability across restarts.

## Decision 3: Apply explicit overwrite semantics in resolver logic (`overwrite = true` required)

- **Decision**: Upload defaults to create-only; if name exists and `overwrite` is not set, return conflict. If `overwrite=true`, replace document content and update metadata atomically.
- **Rationale**: Matches FR-002 and acceptance scenario 2 exactly; prevents accidental destructive writes.
- **Alternatives considered**:
  - Implicit last-write-wins overwrite: rejected as unsafe and non-compliant with FR-002.
  - Separate `updateSchemaDocument` mutation only: rejected because spec explicitly requires upload with overwrite flag retry pattern.

## Decision 4: Resolve concurrent overwrite races as atomic last-successful-write-wins

- **Decision**: For concurrent upload attempts targeting the same `(tenant, name)` with `overwrite=true`, each successful write commits atomically and the final persisted record is whichever write commits last.
- **Rationale**: Aligns with clarified acceptance criteria and FR-014 while preventing partial/interleaved reads.
- **Alternatives considered**:
  - First-writer-wins: rejected because it conflicts with clarified behavior.
  - Global locking that serializes all schema uploads: rejected as unnecessary throughput loss.

## Decision 5: Validate SDL using existing PEGTL GraphQL parser before persistence

- **Decision**: Reuse existing SDL parse validation pattern used by `applyConfiguration` (`gql::generate_ast_and_log`) to reject malformed schema documents.
- **Rationale**: Existing parser path is already integrated and tested in backend; this keeps behavior consistent with current GraphQL SDL validation.
- **Alternatives considered**:
  - Minimal string/regex checks only: rejected as insufficient for SDL well-formedness.
  - Add a second external GraphQL parser dependency: rejected due to unnecessary complexity.

## Decision 6: Resolve schema name/content constraints explicitly

- **Decision**: Enforce schema names as `[A-Za-z0-9._-]{1,128}` with case-sensitive matching. Enforce non-empty content and a deployment-configurable max document size with default `1 MB`.
- **Rationale**: Matches clarified spec decisions exactly and keeps validation deterministic while preventing unbounded payloads.
- **Alternatives considered**:
  - Allow arbitrary UTF-8 names including slashes/spaces: rejected for ambiguous lookup ergonomics and escaping issues.
  - Fixed hard-coded size limit only: rejected because deployments require configurable limits.
  - No content-size limit: rejected due to avoidable memory/latency risk.

## Decision 7: Scope all operations to authenticated context tenant; do not accept caller-provided organization IDs

- **Decision**: For upload/list/fetch resolvers, derive tenant scope from authenticated context only.
- **Rationale**: Reduces context-mismatch attack surface and directly satisfies FR-007 cross-tenant isolation.
- **Alternatives considered**:
  - Accept optional `organizationId` argument with role checks: rejected as unnecessary for this feature and easier to misuse.

## Decision 8: Standardize GraphQL error mapping for deterministic client behavior

- **Decision**: Use structured error codes for failure classes: `UNAUTHENTICATED`, `FORBIDDEN`, `VALIDATION_FAILED`, `CONFLICT`, and null-return for not-found fetch queries.
- **Rationale**: Aligns with existing `EErrorCodes` and gives stable, testable failure handling for clients.
- **Alternatives considered**:
  - Free-form runtime error strings only: rejected because brittle for automation and UI mapping.
  - Return all failures as generic `UNKNOWN_ERROR`: rejected as poor diagnostics and non-compliant with structured error requirement.

## Decision 9: Keep list metadata contract minimal and fixed

- **Decision**: `schemaDocuments` list responses include exactly `name`, `createdAt`, `updatedAt`, and `updatedBy`; `sizeBytes` is excluded.
- **Rationale**: Matches FR-008 and SC-010 and keeps list behavior stable for clients.
- **Alternatives considered**:
  - Include `sizeBytes` by default: rejected because spec explicitly forbids it.
  - Return unbounded metadata blobs: rejected due to contract instability.

## Decision 10: Security documentation and verification are mandatory deliverables

- **Decision**: Produce a feature threat model in `specs/006-upload-schema/threat-model.md` and add feature summary linkage to `docs/security-threat-model.md` during implementation closeout.
- **Rationale**: Constitution mandates threat-model updates for security-sensitive authz/tenant-isolation changes.
- **Alternatives considered**:
  - Defer threat modeling to post-implementation: rejected due to gate risk and incomplete planning evidence.

## Decision 11: Test strategy emphasizes authz, conflict behavior, validation, concurrency, and tenant isolation

- **Decision**: Add focused backend tests that prove upload authorization paths, overwrite conflict behavior, malformed SDL rejection, name/size validation, metadata field contract, last-successful-write-wins concurrency, list/fetch visibility, and cross-tenant isolation.
- **Rationale**: Directly maps to SC-001 through SC-010 and avoids reliance on manual verification.
- **Alternatives considered**:
  - Manual QA-only validation: rejected because outcomes are not repeatable.
  - Unit tests without integration-level tenant-context checks: rejected because isolation/auth outcomes require resolver-level integration evidence.

