---
spec_id: SP-011
title: GraphQL Playground
status: active
owner: isched Development Team
---

# Feature Specification: GraphQL Playground

**Feature Branch**: `011-graphql-playground`  
**Created**: 2026-05-23  
**Status**: Active  
**Input**: User description: "Add a new panel to the UI, the 'GraphQL playground' where a logged in user can see the active schemas and resolvers, preferably in a tree control on the left side. On the right side there will be two panels arranged in a column. On the top panel can the user compose a GraphQL query and on the bottom panel he can see the resolver output. When the user selects a query definition on the left, there should be a button on the bottom of the left panel, which allows to create a default GraphQL query in the top panel on the right side. The UI should allow to resize the panels, left and right, and top and bottom."

## Clarifications

*(to be filled during clarify phase)*

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Browse active schema and resolvers in a tree (Priority: P1)

As a logged-in user, I can open the GraphQL Playground page and see all active schema types, query/mutation/subscription fields, and uploaded schema documents displayed in a collapsible tree on the left panel so I can quickly find what is available.

**Why this priority**: Schema discovery is the foundation of the playground. Without it, query composition and execution have no context.

**Independent Test**: Can be fully tested by navigating to `/playground`, expanding schema tree nodes, and verifying that all Query, Mutation, and Subscription root fields — and at least the schema documents returned by `schemaDocuments` — appear in the tree structure.

**Acceptance Scenarios**:

1. **Given** a logged-in user navigates to `/playground`, **When** the page loads, **Then** a collapsible tree on the left panel is populated with at least Query, Mutation, and Subscription root operation groups and their fields, discovered via standard GraphQL introspection.
2. **Given** the user has uploaded schema documents, **When** the schema tree is displayed, **Then** uploaded schema documents from `schemaDocuments` appear as a distinct group (e.g. "Uploaded Schemas") in the left panel tree.
3. **Given** the tree is populated, **When** the user expands an operation group, **Then** individual fields with their names and return types are visible.
4. **Given** the tree is populated, **When** the user collapses a node, **Then** its children are hidden and the tree stays responsive and readable.

---

### User Story 2 — Compose and execute a GraphQL query (Priority: P1)

As a logged-in user, I can type or paste a GraphQL query into the top-right editor panel and execute it so I can test queries against the live server.

**Why this priority**: Query execution is the central interactive use of a playground and is the primary reason users visit the page.

**Independent Test**: Can be fully tested by loading the playground, typing a valid query (e.g. `{ health { status } }`), running it, and verifying the bottom panel displays a valid JSON response.

**Acceptance Scenarios**:

1. **Given** the playground is open, **When** the user types a valid GraphQL query in the top-right editor and clicks Run, **Then** the query is sent to `/graphql` and the response JSON is displayed in the bottom-right panel.
2. **Given** a query error is returned by the server, **When** the response is displayed, **Then** the bottom panel shows the error message in a clearly distinguished error-styled container.
3. **Given** a query is executing, **When** the response is pending, **Then** the bottom panel shows a loading indicator and the Run button is disabled.
4. **Given** queries are executed in sequence, **When** a new response arrives, **Then** the bottom panel replaces the prior response (no history accumulation without explicit user action).

---

### User Story 3 — Generate a default query from a selected field (Priority: P2)

As a logged-in user, I can select a query, mutation, or subscription field in the left tree and click "Generate Query" at the bottom of the left panel so a default query stub is created in the editor without manual typing.

**Why this priority**: Query generation eliminates the cold-start problem: users unfamiliar with the schema can start exploring with a single click.

**Independent Test**: Can be fully tested by selecting a tree node representing a query field, clicking the Generate button, and verifying the editor contains a valid GraphQL query stub matching the selected field's signature (name and required arguments as placeholder values).

**Acceptance Scenarios**:

1. **Given** the user selects a Query field node in the left tree, **When** the user clicks the "Generate Query" button at the bottom of the left panel, **Then** the top-right editor is populated with a valid GraphQL operation stub for that field.
2. **Given** the selected field has required arguments, **When** the default query is generated, **Then** placeholder argument values appear in the stub (e.g. `id: "..."` for `ID!` fields).
3. **Given** the selected field has no selectable sub-fields (scalar return types), **When** the default query is generated, **Then** the stub is a valid scalar query without a selection set.
4. **Given** no field is selected in the tree, **When** the Generate Query button state is checked, **Then** the button is disabled.

---

### User Story 4 — Resize panels for optimal workspace layout (Priority: P2)

As a logged-in user, I can drag dividers to resize the left/right panels and the top/bottom right panels so I can allocate screen space to match my current task.

**Why this priority**: Panel real-estate matters heavily for wide schemas (more left space) vs. long query results (more bottom space). Fixed-size layouts create friction for different use-cases.

**Independent Test**: Can be fully tested by dragging the vertical divider between the left tree and right panels and the horizontal divider between the editor and result panels, confirming each panel grows or shrinks and its content remains accessible after resize.

**Acceptance Scenarios**:

1. **Given** the playground is displayed, **When** the user drags the vertical divider between the left and right panels, **Then** both panels resize fluidly with no content overlap or broken layout.
2. **Given** the playground is displayed, **When** the user drags the horizontal divider between the query editor (top-right) and result panel (bottom-right), **Then** both panels resize fluidly.
3. **Given** a panel is resized to a minimum, **When** the user releases the divider, **Then** each panel enforces a minimum width/height so no panel becomes invisible.
4. **Given** the page is navigated away and back, **When** the user returns to the playground, **Then** panel sizes either reset to defaults or are remembered from the previous session (implementation decision to be confirmed).

---

### User Story 5 — Confidence through automated tests (Priority: P3)

As a delivery team member, I want unit tests and an end-to-end Playwright test covering the key playground flows so regressions are caught before release.

**Why this priority**: The playground is an interactive, multi-panel component. Automated coverage reduces regression risk during future refactors.

**Independent Test**: Can be fully tested by running Angular unit tests and the Playwright suite and verifying they cover tree population, query generation, query execution result display, and panel resize behavior.

**Acceptance Scenarios**:

1. **Given** the playground component is implemented, **When** Angular unit tests run, **Then** they validate tree population from mocked introspection data, disabled state of Generate Query when no node is selected, correct query stub generation for representative field types, and loading/error/success states in the result panel.
2. **Given** the deployed playground is reachable in a test environment, **When** Playwright end-to-end tests run, **Then** they validate: tree renders operation groups, the Generate Query button inserts a stub, the stub can be executed with a visible response.

### Edge Cases

- Introspection is unavailable or returns an error (e.g. auth failure): the tree shows a non-empty error state and does not render a blank panel.
- The schema document list is empty: the "Uploaded Schemas" group is either hidden or shows a "No uploaded schemas" placeholder.
- A generated query stub contains characters that require escaping in the editor (e.g. quotation marks in default string arguments).
- The user manually edits a generated stub before running it: the edited content is sent as-is without re-generation.
- A query returns a very large JSON payload (> 100 kB): the result panel renders without freezing (virtual scrolling or truncation with expand-all option).
- Panel dividers dragged to their min/max extents must not overflow viewport or break CSS layout.
- Navigating away from the playground mid-execution cancels the pending request or ignores the response on re-entry.
- Subscription operations selected in the tree generate a valid `subscription { ... }` stub; execution of subscriptions is advisory (may show "Subscriptions not supported in playground" if WebSocket execution is out of scope for this iteration).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The WebUI MUST expose a `/playground` route accessible to authenticated users only.
- **FR-002**: The playground MUST be linked from the main navigation menu so users can reach it from any authenticated page.
- **FR-003**: The playground MUST display a resizable left panel containing a collapsible schema tree.
- **FR-004**: The schema tree MUST be populated by standard GraphQL introspection (`__schema` / `__type`) against the `/graphql` endpoint.
- **FR-005**: The schema tree MUST group root operation fields under labelled groups: "Queries", "Mutations", "Subscriptions", each expandable to show individual fields.
- **FR-006**: Each field node in the tree MUST display at minimum the field name and its return type.
- **FR-007**: The schema tree MUST include a distinct "Uploaded Schemas" group populated from the `schemaDocuments` GraphQL query, showing each schema document by name.
- **FR-008**: The playground MUST display a resizable right panel split vertically into a top query-editor sub-panel and a bottom result sub-panel.
- **FR-009**: The query editor MUST be a multi-line text area or dedicated code editor widget supporting syntax highlighting for GraphQL (use CodeMirror 6 or Monaco Editor; decision to be confirmed during planning).
- **FR-010**: The playground MUST provide a "Run" button that sends the current editor content to `/graphql` and displays the JSON response in the result panel.
- **FR-011**: While a query is executing, the Run button MUST be disabled and the result panel MUST show a loading indicator.
- **FR-012**: On a successful response, the result panel MUST display the JSON response with indented formatting.
- **FR-013**: On an error response (GraphQL `errors` field or HTTP error), the result panel MUST display the error in a visually distinguished error state.
- **FR-014**: The playground MUST provide a "Generate Query" button at the bottom of the left panel, enabled only when a Query, Mutation, or Subscription field node is selected.
- **FR-015**: Clicking Generate Query MUST insert a valid GraphQL operation stub for the selected field into the query editor, replacing any existing editor content after user confirmation (or directly if editor is empty).
- **FR-016**: The generated stub MUST include placeholder values for all required arguments, derived from argument types in the introspection result.
- **FR-017**: The vertical divider between the left and right panels MUST be draggable to resize both panels, enforcing minimum widths (≥ 200px each).
- **FR-018**: The horizontal divider between the query editor and result panel MUST be draggable to resize both sub-panels, enforcing minimum heights (≥ 80px each).
- **FR-019**: Panel resize MUST not break the overall page layout or overflow the viewport.

### Frontend Constitutional Requirements *(mandatory when feature includes `src/ui/` changes)*

- **FCR-001**: WebUI state management MUST be signal-first; app-owned template state MUST be signal-backed. Async-pipe-driven template state from component-owned observables is prohibited unless a third-party stream contract requires it and the exception is documented.
- **FCR-002**: All new components, directives, and pipes MUST be standalone and MUST use modern Angular template control flow (`@if`, `@for`, `@switch`).
- **FCR-003**: Templates and styles for all new or refactored components MUST be in separate `.html` and `.scss` files using `templateUrl` and `styleUrl`. No inline `template` or `styles`.
- **FCR-004**: All backend communication MUST use GraphQL `/graphql` only (HTTP for queries/mutations; WebSocket for subscriptions if in scope). No REST calls.
- **FCR-005**: JWT handling MUST avoid persistent token storage (`localStorage`/`sessionStorage`/IndexedDB).
- **FCR-006**: Local development behavior MUST preserve Angular dev-server proxy routing for `/graphql` (HTTP + WebSocket).
- **FCR-007**: UI styling MUST follow Tailwind CSS 3.x + DaisyUI 4.x conventions, using DaisyUI component classes as the baseline and extending with Tailwind utilities.

### Key Entities *(include if feature involves data)*

- **PlaygroundPage**: The top-level Angular standalone route component at `/playground`.
- **SchemaTreeComponent**: Left-panel collapsible tree component that displays operation groups and fields discovered via GraphQL introspection.
- **SchemaTreeNode**: Data model for a tree node — types: `operationGroup` (Queries/Mutations/Subscriptions/UploadedSchemas), `field`, `schemaDocument`.
- **QueryEditorComponent**: Top-right editor panel wrapping a code editor widget (CodeMirror 6 or Monaco).
- **ResultPanelComponent**: Bottom-right panel rendering JSON response, loading state, or error state.
- **ResizableSplitComponent** (or equivalent): General-purpose container managing a draggable divider between two child panels (used twice: vertical L/R split, horizontal T/B split in right area).
- **IntrospectionService**: Angular service that performs GraphQL introspection via `graphql.service.ts` and maps the result to `SchemaTreeNode[]`.
- **PlaygroundQueryService**: Angular service that sends ad-hoc GraphQL queries from the editor to `/graphql` and returns the result as a signal-compatible observable or promise.

### Assumptions

- The playground is only accessible to authenticated users; accessing `/playground` while unauthenticated redirects to `/login`.
- GraphQL introspection is enabled on the server endpoint (no `__schema` disable flag); if introspection is disabled a clear error is shown.
- The schema tree shows the server's currently active combined schema (built-in + uploaded/activated tenant schema merge) as seen through introspection.
- Uploaded schema documents from `schemaDocuments` are shown by name only in the tree; clicking a document node may show its SDL in the result panel but full document editing is out of scope for this feature.
- Subscription execution via WebSocket in the playground is deferred (out of scope for initial iteration); selecting a subscription field generates a stub but Run shows an advisory "Subscriptions not yet supported in playground" message.
- Panel size persistence across navigation is nice-to-have; the implementation phase will decide whether to use localStorage signals or reset to defaults.
- The code editor widget choice (CodeMirror 6 vs. Monaco) will be finalized during implementation planning; both satisfy the spec requirement. CodeMirror 6 is preferred due to its smaller bundle size.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The `/playground` route is guarded and redirects unauthenticated users to `/login` in all automated test runs.
- **SC-002**: In acceptance testing, the schema tree populates at least the Query, Mutation, and Subscription operation groups from live introspection and matches the fields defined in `isched_builtin_server_schema.graphql`.
- **SC-003**: An end-to-end Playwright test executes the flow: navigate to playground → generate query from `health` field → run query → verify result panel shows `{ "data": { "health": ... } }` response.
- **SC-004**: All Angular unit tests for playground components pass with ≥ 80% branch coverage on PlaygroundPage, SchemaTreeComponent, ResultPanelComponent, and IntrospectionService.
- **SC-005**: Panel resize operations in Playwright tests do not produce JavaScript errors or broken layout at three representative viewport widths (1280px, 1024px, 768px).
- **SC-006**: The Generate Query button is disabled when no tree node is selected and enabled when a field node is selected, verified in unit tests.
- **SC-007**: Before release, `ctest --output-on-failure` passes 100% and `ng test` passes 100%.

