# Research: GraphQL Playground

## Decision 1: Build the playground as a frontend-first feature on top of the existing GraphQL stack

- **Decision**: Implement the playground UI in Angular and consume the existing `/graphql` endpoint for introspection and query execution; avoid introducing any REST or alternate transport.
- **Rationale**: The repository already standardizes on GraphQL-only frontend access, and the feature spec is centered on query discovery and execution rather than new backend workflows.
- **Alternatives considered**:
  - New REST endpoints for schema browsing: rejected because it would violate the GraphQL-only frontend contract.
  - Backend-rendered playground page: rejected because the rest of the WebUI is Angular standalone.

## Decision 2: Represent schema discovery as a normalized tree built from full introspection plus uploaded schema documents

- **Decision**: Fetch the full introspection graph and `schemaDocuments`, then merge both sources into one tree model for the left panel.
- **Rationale**: The clarified requirement is to show the full introspection graph, and uploaded schemas must appear in the main tree instead of a separate group. A normalized tree model keeps rendering and selection logic simple.
- **Alternatives considered**:
  - Separate tree sections for uploaded schemas: rejected by clarification.
  - Lazy fetch per tree branch: rejected for this feature because the clarified scope asks for the full introspection tree.

## Decision 3: Use CodeMirror 6 for the query editor with GraphQL syntax support

- **Decision**: Implement the top-right editor with CodeMirror 6 and the GraphQL language package/theme integration needed for syntax highlighting.
- **Rationale**: This matches the clarified editor choice, aligns with the current Angular SPA stack, and keeps the editor isolated behind a standalone component.
- **Alternatives considered**:
  - Monaco Editor: rejected because it is larger than needed for a simple GraphQL playground.
  - Plain textarea: rejected because it would not satisfy the editor experience or syntax highlighting expectations.

## Decision 4: Generate subscription stubs, but keep execution advisory-only

- **Decision**: Allow selecting subscription fields and generating `subscription { ... }` stubs, but do not execute subscriptions in this iteration; Run should show a clear advisory message.
- **Rationale**: This matches the clarified scope and avoids adding WebSocket execution flows that are not needed for the current user journey.
- **Alternatives considered**:
  - Full WebSocket subscription execution: rejected as out of scope for this feature iteration.
  - Hiding subscriptions entirely: rejected because the feature still needs to help users discover available operations.

## Decision 5: Persist splitter sizes in browser storage and restore them on load

- **Decision**: Store left/right and top/bottom panel sizes in browser-persistent UI preferences and restore them during playground initialization.
- **Rationale**: The feature explicitly requires persistence across browser sessions, and the layout state is user preference data rather than sensitive session data.
- **Alternatives considered**:
  - In-memory-only state: rejected because it loses preferences across sessions.
  - Server-side persistence: rejected because the layout is local UI preference data and does not need backend storage.

## Decision 6: Model the UI around small standalone Angular components with signal-backed view state

- **Decision**: Split the playground into standalone route, tree, editor, result panel, and reusable resizable split components, with signals for selection/loading/error/layout state.
- **Rationale**: This matches the Angular constitution, keeps each piece focused, and makes testing the panel behaviors straightforward.
- **Alternatives considered**:
  - Single large page component: rejected because it would become difficult to test and maintain.
  - Observable-driven template state: rejected by the repository's signal-first Angular guidance.

