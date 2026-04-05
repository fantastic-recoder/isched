# Feature Specification: Tenant Admin Schema Upload

**Feature Branch**: `006-upload-schema`  
**Created**: 2026-04-05  
**Status**: Ready for Planning  
**Input**: User description: "A member of a tenant_admin group has the right to upload GraphQL schema documents. The documents will be visible on the organization scope. Their names are organization unique."

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

---

### User Story 2 - List Available Schemas (Priority: P2)

As an organization member, I need to see which schema documents are available in my organization so that I know what schemas have been uploaded and can reference them.

**Why this priority**: Schemas are only useful if members can discover what exists. Listing is the minimal read-access surface required for organization-level visibility.

**Independent Test**: Can be fully tested independently by uploading one or more schemas as a tenant_admin, then querying the list as a regular organization member and verifying names and metadata appear.

**Acceptance Scenarios**:

1. **Given** at least one schema has been uploaded in my organization, **When** I request the list of schemas as any authenticated organization member, **Then** I see the names (and basic metadata) of all schemas belonging to my organization.
2. **Given** no schemas have been uploaded yet, **When** an authenticated organization member requests the list, **Then** the system returns an empty list without error.
3. **Given** I am authenticated in organization A, **When** I query schemas, **Then** I only see schemas belonging to organization A and never schemas from other organizations.

---

### User Story 3 - Retrieve a Specific Schema Document (Priority: P3)

As an organization member, I need to retrieve the content of a specific schema document by name so that I can read or use its contents.

**Why this priority**: Once schemas can be listed, members should be able to fetch the full content of a specific schema. This completes the read path started by listing.

**Independent Test**: Can be fully tested by uploading a schema and then fetching it by name as any authenticated organization member, verifying the returned content matches what was uploaded.

**Acceptance Scenarios**:

1. **Given** a schema with a known name exists in my organization, **When** I request it by name, **Then** the system returns the full schema document content.
2. **Given** no schema with the requested name exists in my organization, **When** I request it by name, **Then** the system returns a clear "not found" response.
3. **Given** I am authenticated in organization A, **When** I fetch a schema by name, **Then** the lookup is scoped to organization A and cannot resolve schema names from other organizations.

---

### Edge Cases

- What happens when a schema document body is empty?
- What happens when a schema name is an empty string or contains only whitespace?
- How does the system behave when a tenant_admin uploads a very large schema document?
- What is the behavior when the organization has no schemas and a listing is requested?
- Can a schema name contain special characters (e.g., spaces, slashes)?
- Are schema names treated as case-sensitive within the organization uniqueness constraint?

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST allow authenticated members of the `tenant_admin` group to upload GraphQL schema documents to their organization via the GraphQL API.
- **FR-002**: The system MUST enforce organization-scoped uniqueness on schema document names. If a schema with the requested name already exists in the organization and no explicit overwrite flag is provided, the upload MUST be rejected with a structured name-collision error. If the overwrite flag is explicitly set, the existing schema document MUST be replaced with the new content.
- **FR-003**: The system MUST validate that uploaded schema document content is well-formed before accepting and persisting it; invalid content MUST be rejected with a descriptive error.
- **FR-004**: The system MUST reject schema upload attempts from any authenticated user who is not a member of the `tenant_admin` group, returning an authorization error.
- **FR-005**: The system MUST reject schema upload attempts from unauthenticated callers with an authentication error.
- **FR-006**: The system MUST make uploaded schema documents visible (readable and listable) to all authenticated members of the same organization.
- **FR-007**: The system MUST ensure schema documents from one organization are never accessible to members of a different organization.
- **FR-008**: The system MUST allow any authenticated organization member to list all schema document names and metadata belonging to their organization.
- **FR-009**: The system MUST allow any authenticated organization member to retrieve the full content of a schema document by its organization-unique name.
- **FR-010**: The system MUST reject schema uploads where the name is empty, contains only whitespace, or violates defined naming constraints, returning a descriptive validation error.
- **FR-011**: The system MUST persist schema documents durably so they survive server restarts.

### Frontend Constitutional Requirements *(mandatory when feature includes `src/ui/` changes)*

- **FCR-001**: WebUI state management MUST be signal-first; app-owned template state MUST be signal-backed, and async-pipe-driven template state from component-owned observables is prohibited unless a third-party stream contract requires it and the exception is documented.
- **FCR-002**: New UI elements MUST use standalone components/directives/pipes and modern Angular template control flow (`@if`, `@for`, `@switch`).
- **FCR-003**: User input flows MUST use typed reactive forms with strict TypeScript and strict template type checking enabled.
- **FCR-004**: Browser API consumption MUST use GraphQL `/graphql` only (HTTP/WebSocket) with no REST fallback.
- **FCR-005**: JWT handling MUST avoid persistent token storage (`localStorage`/`sessionStorage`/IndexedDB) and define secure transport/storage controls.
- **FCR-006**: Local development MUST define Angular dev-server proxy behavior for `/graphql` (including WebSocket upgrades).

### Key Entities

- **SchemaDocument**: A named GraphQL schema document owned by an organization. Key attributes: organization-unique name, document content (SDL text), owner organization, upload timestamp, uploader identity.
- **Organization (Tenant)**: The scoping boundary for schema document names and visibility. A schema document belongs to exactly one organization.
- **SchemaUploadOutcome**: The result of an upload attempt, indicating success (with schema reference) or failure (with a structured error reason covering authorization, name collision, or content validation failure).

### Assumptions

- "Organization" and "tenant" are used interchangeably; the uniqueness and visibility boundary is the tenant.
- Schema document content is expected to be GraphQL SDL (Schema Definition Language) text.
- Schema names are treated as case-sensitive strings unless a future requirement specifies otherwise.
- Schema names may not be empty or whitespace-only; other naming constraints (max length, allowed characters) follow reasonable defaults.
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
- **SC-006**: In automated end-to-end upload-then-fetch tests, 100% of successfully uploaded schema documents can be retrieved by name with content identical to what was uploaded.
- **SC-007**: Tenant admin users can complete a schema upload workflow (name entry, content submission, confirmation) in under 2 minutes on a standard connection.
