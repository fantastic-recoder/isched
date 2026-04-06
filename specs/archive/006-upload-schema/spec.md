# Feature Specification: Tenant Admin Schema Upload

**Feature Branch**: `006-upload-schema`  
**Created**: 2026-04-05  
**Status**: Ready for Planning  
**Input**: User description: "A member of a tenant_admin group has the right to upload GraphQL schema documents. The documents will be visible on the organization scope. Their names are organization unique."

## Clarifications

### Session 2026-04-06

- Q: What schema-name format is allowed? → A: Schema names MUST match `[A-Za-z0-9._-]{1,128}`.
- Q: Are schema names case-sensitive for uniqueness and lookup? → A: Yes, schema names are case-sensitive.
- Q: What is the maximum allowed schema document size? → A: Max document size is configurable, default 1 MB.
- Q: For concurrent uploads with overwrite enabled targeting the same schema name, what conflict resolution rule applies? → A: Last successful write wins atomically by commit order.
- Q: Which metadata fields are returned by schema listing? → A: `name`, `createdAt`, `updatedAt`, `updatedBy` (no `sizeBytes`).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Upload a New Schema Document (Priority: P1)

As a tenant admin, I need to upload a named GraphQL schema document to my organization so that it becomes available to all organization members.

**Why this priority**: This is the core action of the feature. Without successful upload, nothing else in the feature delivers value.

**Independent Test**: Can be fully tested by a tenant_admin uploading a schema document and verifying it is stored under the given name and readable by organization members.

**Acceptance Scenarios**:

1. **Given** I am authenticated as a tenant_admin and no schema with my chosen name exists, **When** I upload a valid schema document with a unique name, **Then** the schema is stored and accessible to all organization members under that name.
2. **Given** I am authenticated as a tenant_admin and a schema with my chosen name already exists in my organization, **When** I attempt to upload without providing an explicit overwrite confirmation, **Then** the system rejects the upload with a structured name-collision error that identifies the conflicting name; **When** I retry with an explicit overwrite flag set, **Then** the existing schema document is replaced with the new content and the operation succeeds.
3. **Given** I am authenticated as a tenant_admin, **When** I upload a schema with invalid or malformed content, **Then** the system rejects the upload and returns a clear error describing why the content is invalid.
4. **Given** I am authenticated but do **not** belong to the tenant_admin group, **When** I attempt to upload a schema document, **Then** the system denies the operation with an authorization error.
5. **Given** two tenant_admin users concurrently upload different contents for the same schema name with explicit overwrite enabled, **When** both uploads complete successfully, **Then** the final stored content reflects the last successful commit order and no partial/interleaved document is ever visible.

---

### User Story 2 - List Available Schemas (Priority: P2)

As an organization member, I need to see which schema documents are available in my organization so that I know what schemas have been uploaded and can reference them.

**Why this priority**: Schemas are only useful if members can discover what exists. Listing is the minimal read-access surface required for organization-level visibility.

**Independent Test**: Can be fully tested independently by uploading one or more schemas as a tenant_admin, then querying the list as a regular organization member and verifying names and metadata appear.

**Acceptance Scenarios**:

1. **Given** at least one schema has been uploaded in my organization, **When** I request the list of schemas as any authenticated organization member, **Then** I see all schemas belonging to my organization with metadata fields `name`, `createdAt`, `updatedAt`, and `updatedBy`.
2. **Given** no schemas have been uploaded yet, **When** an authenticated organization member requests the list, **Then** the system returns an empty list without error.
3. **Given** I am authenticated in organization A, **When** I query schemas, **Then** I only see schemas belonging to organization A and never schemas from other organizations.

---

### User Story 3 - Retrieve a Specific Schema Document (Priority: P3)

As an organization member, I need to retrieve the content of a specific schema document by name so that I can read or use its contents.

**Why this priority**: Once schemas can be listed, members should be able to fetch the full content of a specific schema. This completes the read path started by listing.

**Independent Test**: Can be fully tested by uploading a schema and then fetching it by name as any authenticated organization member, verifying the returned content matches what was uploaded.

**Acceptance Scenarios**:

1. **Given** a schema with a known name exists in my organization, **When** I request it by name, **Then** the system returns the full schema document content.
2. **Given** no schema with the requested name exists in my organization, **When** I request it by name, **Then** the system returns `null` for the schema field (deterministic null-on-miss behavior) and does not return a GraphQL error for the miss.
3. **Given** I am authenticated in organization A, **When** I fetch a schema by name, **Then** the lookup is scoped to organization A and cannot resolve schema names from other organizations.

---

### Edge Cases

- Uploads with an empty schema document body are rejected with a descriptive validation error.
- Uploads with an empty schema name or a schema name containing only whitespace are rejected with a descriptive validation error.
- Uploads that exceed the configured maximum schema document size are rejected with a descriptive validation error (default maximum: 1 MB).
- When an organization has no schemas, list requests return an empty list without error.
- Names containing characters outside `[A-Za-z0-9._-]` (including spaces and slashes) are rejected with a validation error.
- Schema names are case-sensitive for organization-scoped uniqueness and by-name retrieval.
- Fetching by a schema name that does not exist in the caller's organization returns `null` deterministically (null-on-miss).
- Concurrent overwrite-enabled uploads for the same schema name resolve by atomic last-successful-write-wins commit order.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST allow authenticated members of the `tenant_admin` group to upload GraphQL schema documents to their organization via the GraphQL API.
- **FR-002**: The system MUST enforce organization-scoped uniqueness on schema document names. If a schema with the requested name already exists in the organization and no explicit overwrite flag is provided, the upload MUST be rejected with a structured name-collision error. If the overwrite flag is explicitly set, the existing schema document MUST be replaced with the new content.
- **FR-003**: The system MUST validate that uploaded schema document content is well-formed before accepting and persisting it; invalid content MUST be rejected with a descriptive error.
- **FR-004**: The system MUST reject schema upload attempts from any authenticated user who is not a member of the `tenant_admin` group, returning an authorization error.
- **FR-005**: The system MUST reject schema upload attempts from unauthenticated callers with an authentication error.
- **FR-006**: The system MUST make uploaded schema documents visible (readable and listable) to all authenticated members of the same organization.
- **FR-007**: The system MUST ensure schema documents from one organization are never accessible to members of a different organization.
- **FR-008**: The system MUST allow any authenticated organization member to list all schema documents belonging to their organization, and each list item MUST include exactly the metadata fields `name`, `createdAt`, `updatedAt`, and `updatedBy` (and MUST NOT include `sizeBytes`).
- **FR-009**: The system MUST allow any authenticated organization member to retrieve the full content of a schema document by its organization-unique name.
- **FR-010**: The system MUST reject schema uploads unless the name matches all of the following constraints: length `1..128`, characters limited to `[A-Za-z0-9._-]`; violations MUST return a descriptive validation error.
- **FR-011**: The system MUST persist schema documents durably so they survive server restarts, and this durability MUST be verified by an automated scenario that uploads a schema, performs a controlled restart, and confirms the same schema remains retrievable in the same organization after restart.
- **FR-012**: Schema-name uniqueness and by-name retrieval matching MUST be case-sensitive (for example, `Billing`, `billing`, and `BILLING` are distinct names within the same organization).
- **FR-013**: The system MUST enforce a maximum upload size for schema document content that is configurable by deployment; if not explicitly configured, the default limit MUST be `1 MB`. Uploads exceeding the active limit MUST be rejected with a descriptive validation error.
- **FR-014**: When multiple overwrite-enabled uploads target the same organization/name concurrently, each successful upload MUST be committed atomically, and the final persisted document MUST be the one from the last successful commit order (last-successful-write-wins).
- **FR-015**: For fetch-by-name requests where the schema does not exist in the caller's organization, the system MUST return `null` for the requested schema field (canonical deterministic null-on-miss behavior) and MUST NOT emit a GraphQL error solely for that miss.

### Frontend Constitutional Requirements *(mandatory when feature includes `src/ui/` changes)*

- **FCR-001**: WebUI state management MUST be signal-first; app-owned template state MUST be signal-backed, and async-pipe-driven template state from component-owned observables is prohibited unless a third-party stream contract requires it and the exception is documented.
- **FCR-002**: New UI elements MUST use standalone components/directives/pipes and modern Angular template control flow (`@if`, `@for`, `@switch`).
- **FCR-003**: User input flows MUST use typed reactive forms with strict TypeScript and strict template type checking enabled.
- **FCR-004**: Browser API consumption MUST use GraphQL `/graphql` only (HTTP/WebSocket) with no REST fallback.
- **FCR-005**: JWT handling MUST avoid persistent token storage (`localStorage`/`sessionStorage`/IndexedDB) and define secure transport/storage controls.
- **FCR-006**: Local development MUST define Angular dev-server proxy behavior for `/graphql` (including WebSocket upgrades).

### Key Entities

- **SchemaDocument**: A named GraphQL schema document owned by an organization. Key attributes: organization-unique name (regex `[A-Za-z0-9._-]{1,128}`), document content (SDL text), owner organization, `createdAt`, `updatedAt`, and `updatedBy`.
- **Organization (Tenant)**: The scoping boundary for schema document names and visibility. A schema document belongs to exactly one organization.
- **SchemaUploadOutcome**: The result of an upload attempt, indicating success (with schema reference) or failure (with a structured error reason covering authorization, name collision, or content validation failure).

### Assumptions

- "Organization" and "tenant" are used interchangeably; the uniqueness and visibility boundary is the tenant.
- Schema document content is expected to be GraphQL SDL (Schema Definition Language) text.
- Schema-name matching is case-sensitive for uniqueness checks and retrieval.
- Schema names MUST match `[A-Za-z0-9._-]{1,128}`.
- Maximum schema document size is deployment-configurable; default is `1 MB` when no override is provided.
- This feature covers create and read operations only; update and delete are out of scope for this iteration.
- Read access (list and fetch) is open to all authenticated organization members, not restricted to tenant_admin.

### Dependencies

- JWT-based authentication and group membership resolution (tenant_admin group check) must be available at the time of schema upload.
- Per-tenant persistent storage must support durable document storage scoped to each tenant.
- Existing GraphQL transport layer (`/graphql` endpoint) must be operational.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In automated authorization tests, 100% of upload attempts by non-tenant_admin members are rejected with an authorization error.
- **SC-002**: In automated upload tests, 100% of uploads with a name duplicating an existing organization schema name and no overwrite flag are rejected with a structured name-collision error; 100% of the same uploads retried with the overwrite flag set succeed and replace the existing document.
- **SC-003**: In automated content-validation tests, 100% of uploads with malformed or empty schema content are rejected with a descriptive validation error.
- **SC-004**: In automated cross-tenant isolation tests, 0% of schema list or fetch operations return documents belonging to a different organization.
- **SC-005**: In automated end-to-end upload-then-list tests, 100% of newly uploaded schemas appear in the organization's schema list immediately after a successful upload.
- **SC-006**: In automated upload-then-fetch durability tests, 100% of successfully uploaded schema documents can be retrieved by name with content identical to what was uploaded both before and after a controlled backend restart.
- **SC-007**: In automated backend integration performance tests, at least 95% of valid upload requests (document size at or below active limit) complete with a definitive GraphQL outcome in under 2 seconds, measured at the backend request boundary.
- **SC-008**: In automated size-limit tests, 100% of uploads with document size above the active configured limit are rejected with a descriptive validation error, and 100% of uploads at or below the limit are evaluated normally; when no limit is configured, the default `1 MB` limit is applied.
- **SC-009**: In automated concurrency tests with at least two overwrite-enabled uploads racing on the same organization/name, the final stored schema content always equals the payload from the last successful commit, and no read ever observes partial/interleaved content.
- **SC-010**: In automated list-schema contract tests, 100% of returned list items include `name`, `createdAt`, `updatedAt`, and `updatedBy`, and 0% include `sizeBytes`.
