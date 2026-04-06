# Data Model: Tenant Admin Schema Upload

**Phase**: 1 (Design & Contracts)  
**Updated**: 2026-04-06  
**Feature**: `006-upload-schema`

## Entity: SchemaDocument

**Purpose**: Durable GraphQL SDL document stored per tenant and visible to authenticated members of that tenant.

**Fields**:
- `tenantId: String` (derived from auth context; not client-supplied)
- `name: String` (organization-unique identifier, case-sensitive)
- `content: String` (GraphQL SDL text)
- `contentSha256: String` (hash of `content` for change/audit diagnostics)
- `createdAt: DateTime`
- `updatedAt: DateTime`
- `updatedBy: String`

**Validation Rules**:
- `name` MUST match `[A-Za-z0-9._-]{1,128}`.
- `name` matching for uniqueness and lookup is case-sensitive (`Billing` != `billing`).
- `content` MUST be non-empty and <= active configured max size.
- Active max size defaults to `1 MB` when not configured by deployment.
- `content` MUST parse as well-formed GraphQL SDL before persistence.
- `(tenantId, name)` MUST be unique logically (implemented by tenant-local DB + `name` key).

**Relationships**:
- Belongs to exactly one tenant (`tenantId` from authenticated context).
- Created/updated by one authenticated user (`updatedBy`).

## Entity: SchemaUploadRequest

**Purpose**: GraphQL mutation input payload for schema upload behavior.

**Fields**:
- `name: String`
- `content: String`
- `overwrite: Boolean` (default `false`)

**Validation Rules**:
- `name` and `content` required.
- `overwrite=false` with existing `name` MUST fail with conflict error.
- `overwrite=true` with existing `name` MUST replace stored `content` and update metadata atomically.
- For concurrent `overwrite=true` writes to same `(tenantId, name)`, final stored content is the last successful commit (last-successful-write-wins).

## Entity: SchemaUploadOutcome

**Purpose**: Structured response envelope for upload mutation result.

**Fields**:
- `success: Boolean`
- `schema: SchemaDocumentSummary?`
- `errorCode: Enum(UNAUTHENTICATED, FORBIDDEN, VALIDATION_FAILED, CONFLICT, UNKNOWN_ERROR)?`
- `message: String?`
- `conflictingName: String?` (present for conflict path)

**Validation Rules**:
- `success=true` MUST include `schema` and no `errorCode`.
- `success=false` MUST include `errorCode` and human-readable `message`.
- `errorCode=CONFLICT` MUST include `conflictingName`.

## Entity: SchemaDocumentSummary

**Purpose**: Lightweight metadata projection for list results.

**Fields**:
- `name: String`
- `createdAt: DateTime`
- `updatedAt: DateTime`
- `updatedBy: String`

**Validation Rules**:
- List results MUST include exactly `name`, `createdAt`, `updatedAt`, `updatedBy` (no additional metadata fields such as `sizeBytes`).
- List results MUST include only records for caller tenant.
- Empty tenant store MUST return an empty list, not an error.

## Entity: SchemaLookupRequest

**Purpose**: Input for fetching a single schema document by name.

**Fields**:
- `name: String`

**Validation Rules**:
- `name` required and validated with same naming constraints as upload.
- Name lookup is case-sensitive using exact identifier semantics.
- Lookup is tenant-scoped to auth context.
- Missing document returns `null` deterministically for the `schemaDocument` field and does not emit a GraphQL error for the miss.

## State Transitions: SchemaDocument Lifecycle

- `Absent -> Created`: valid upload with unique `name` and `overwrite=false`.
- `Created -> Replaced`: valid upload with same `name` and `overwrite=true`.
- `Created/Replaced -> Retrieved`: successful tenant-scoped fetch by `name`.
- `Created/Replaced -> Listed`: included in tenant-scoped list query results.

## Derived Invariants

- No caller outside `tenant_admin` can create or overwrite documents.
- Any authenticated member of a tenant can list/fetch documents for that tenant.
- No list/fetch/upload path can read or mutate another tenant's records.
- Upload persistence survives process restart (SQLite durability requirement).

