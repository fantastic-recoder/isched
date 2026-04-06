# GraphQL Contract: Tenant Schema Document Upload and Read

**Feature**: `006-upload-schema`  
**Transport**: GraphQL over `/graphql` (HTTP + WebSocket where applicable)

## Contract Goals

- Provide tenant-admin upload with explicit overwrite semantics.
- Provide authenticated tenant-member list and fetch access.
- Keep strict tenant isolation and structured GraphQL error codes.

## Operations in Scope

### 1) Upload schema document (tenant_admin only)

```graphql
mutation UploadSchemaDocument($input: UploadSchemaDocumentInput!) {
  uploadSchemaDocument(input: $input) {
    success
    schema {
      name
      createdAt
      updatedAt
      updatedBy
    }
    error {
      code
      message
      conflictingName
    }
  }
}
```

```graphql
input UploadSchemaDocumentInput {
  name: String!
  content: String!
  overwrite: Boolean = false
}
```

**Behavioral Contract**:
- Caller must be authenticated and have `tenant_admin` role.
- `overwrite` defaults to `false`.
- `name` must match `[A-Za-z0-9._-]{1,128}` and matching is case-sensitive.
- `content` must be non-empty and must not exceed the active configured size limit (default `1 MB`).
- If `name` exists and `overwrite=false`, operation returns conflict payload/error metadata identifying the conflicting name.
- If `name` exists and `overwrite=true`, content is replaced atomically and mutation succeeds.
- For concurrent `overwrite=true` writes on the same name, final persisted content follows last-successful-write-wins by commit order.
- Persisted schema documents must remain retrievable after a controlled backend restart.
- Invalid name/content must fail with validation error.

### 2) List schema documents (any authenticated tenant member)

```graphql
query ListSchemaDocuments {
  schemaDocuments {
    name
    createdAt
    updatedAt
    updatedBy
  }
}
```

**Behavioral Contract**:
- Returns all schema documents for caller tenant only.
- Returns empty list if tenant has no schema documents.
- Never leaks schema names from other tenants.
- List metadata fields are `name`, `createdAt`, `updatedAt`, and `updatedBy` only (`sizeBytes` is not returned).

### 3) Fetch schema document by name (any authenticated tenant member)

```graphql
query GetSchemaDocument($name: String!) {
  schemaDocument(name: $name) {
    name
    content
    createdAt
    updatedAt
    updatedBy
  }
}
```

**Behavioral Contract**:
- Lookup is tenant-scoped using authenticated context.
- Unknown `name` returns `null` deterministically in the `schemaDocument` field.
- A not-found miss must not produce a GraphQL error entry by itself.
- Never resolves another tenant's document, even for same `name`.

Canonical not-found fetch response:

```json
{
  "data": {
    "schemaDocument": null
  }
}
```

## Error Envelope Contract

Canonical authorization failure:

```json
{
  "errors": [
    {
      "message": "Forbidden",
      "extensions": {
        "code": "FORBIDDEN"
      }
    }
  ]
}
```

Canonical authentication failure:

```json
{
  "errors": [
    {
      "message": "Authentication required",
      "extensions": {
        "code": "UNAUTHENTICATED"
      }
    }
  ]
}
```

Canonical name-collision conflict:

```json
{
  "errors": [
    {
      "message": "Schema document name already exists",
      "extensions": {
        "code": "CONFLICT",
        "conflictingName": "billing-v1"
      }
    }
  ]
}
```

Canonical validation failure (name/content/SDL):

```json
{
  "errors": [
    {
      "message": "Invalid schema document content",
      "extensions": {
        "code": "VALIDATION_FAILED"
      }
    }
  ]
}
```

## Compatibility Rules

- `/graphql` remains the only external API surface.
- Existing auth and role model stays unchanged (`role_tenant_admin` for write operations).
- Operations are tenant-context driven and do not require caller-supplied `organizationId`.
- Name uniqueness is organization-scoped and case-sensitive.
- Schema listing metadata is fixed to `name`, `createdAt`, `updatedAt`, and `updatedBy`.

