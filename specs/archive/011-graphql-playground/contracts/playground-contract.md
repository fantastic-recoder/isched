# GraphQL Playground Contract

## Route Contract

- **Path**: `/playground`
- **Access**: authenticated users only
- **Redirect behavior**: unauthenticated users are redirected to `/login`
- **Shell integration**: the route is reachable from the authenticated navigation menu

## UI Contract

### Left panel
- Renders a collapsible schema tree.
- Tree contents include:
  - Query, Mutation, and Subscription operation groups
  - object, input, enum, directive, interface, and scalar/type nodes needed for schema browsing
  - uploaded schema document nodes merged into the same tree
- Field nodes show at minimum:
  - field name
  - return type
- Only Query, Mutation, and Subscription field nodes are selectable for generation.

### Right panel, top
- Contains the GraphQL query editor.
- Editor technology: CodeMirror 6.
- Supports GraphQL syntax highlighting.
- Receives generated stubs and user edits.

### Right panel, bottom
- Renders one of four states:
  - loading indicator
  - formatted JSON success response
  - error state
  - advisory state for subscriptions
- Run replaces the previous visible response with the newest result.

## GraphQL Interaction Contract

### Introspection
- The playground fetches schema information via `/graphql` only.
- The tree is built from the full introspection graph plus `schemaDocuments`.
- Uploaded schema documents are merged into the main tree model and not grouped separately.

### Query execution
- Run posts the current editor contents to `/graphql`.
- Successful responses are rendered as indented JSON.
- GraphQL error responses and HTTP errors are rendered as visible error states.
- The Run button is disabled while a request is in flight.

### Subscriptions
- Subscription fields generate a valid stub.
- Run on a subscription stub shows an advisory message instead of executing a WebSocket subscription.

## Layout Contract

- The left/right split is draggable.
- The right top/bottom split is draggable.
- Minimum sizes are enforced so no panel collapses out of view.
- Split sizes persist across browser sessions and are restored on load.

## Test Contract

- Unit tests cover tree normalization, selection state, query stub generation, loading/error/success rendering, and layout persistence behavior.
- Playwright coverage verifies route access, tree rendering, query generation, query execution, and resize interactions.

