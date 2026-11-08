# Data Model: LSP GraphQL Angular Integration

## Entity: GraphQLOperation
- **Description**: Client-issued GraphQL query/mutation/subscription for the primary UI flow.
- **Fields**:
  - `operationName` (string, required)
  - `operationType` (enum: `query|mutation|subscription`, required)
  - `variables` (typed object/map, required per operation contract)
  - `result` (typed payload or error envelope)
  - `issuedAt` (timestamp)
- **Validation Rules**:
  - operation name must match declared contract entry
  - variables must satisfy typed schema and non-null constraints

## Entity: AuthSessionContext
- **Description**: Browser-runtime auth state for current active session.
- **Fields**:
  - `isAuthenticated` (boolean)
  - `principal` (identity claims subset)
  - `authMaterial` (ephemeral token/cookie context reference)
  - `expiresAt` (timestamp/nullable)
- **Validation Rules**:
  - no persistence to localStorage/sessionStorage/IndexedDB
  - lifecycle must clear auth material on logout/session invalidation

## Entity: ProxyRouteConfig
- **Description**: Angular dev-server routing rules for GraphQL traffic.
- **Fields**:
  - `route` (literal `/graphql`)
  - `targetOrigin` (backend origin)
  - `ws` (boolean, must be true for subscription support)
  - `secure` (boolean/environment-specific)
- **Validation Rules**:
  - route must include HTTP forwarding and WebSocket upgrade support
  - frontend source must not embed environment-specific backend hostnames

## Entity: UiFlowState
- **Description**: Signal-backed UI state for primary feature journey.
- **Fields**:
  - `status` (enum: `idle|submitting|success|error|reauth-required`)
  - `formValue` (typed reactive form model)
  - `errorMessage` (string/nullable)
  - `canRetry` (boolean)
- **State Transitions**:
  - `idle -> submitting -> success`
  - `idle|error -> submitting`
  - `submitting -> error|reauth-required`

## Entity: GraphQLErrorPresentation
- **Description**: User-facing representation of GraphQL/transport/auth errors.
- **Fields**:
  - `category` (enum: `validation|authorization|transport|unknown`)
  - `message` (localized/display-safe string)
  - `retryable` (boolean)
- **Validation Rules**:
  - message must be actionable and non-destructive
  - retryable must match category policy
