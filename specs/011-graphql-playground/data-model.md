# Data Model: GraphQL Playground

## Entities

### PlaygroundPageState
- **Purpose**: Top-level page state for `/playground`.
- **Fields**:
  - `isLoadingTree: boolean`
  - `loadError: string | null`
  - `selectedNodeId: string | null`
  - `canGenerateQuery: boolean`
  - `isRunningQuery: boolean`
  - `resultMode: 'idle' | 'loading' | 'success' | 'error' | 'advisory'`
  - `resultPayload: unknown | null`
  - `resultMessage: string | null`
- **Relationships**:
  - Owns the current tree, editor content, and result state.

### SchemaTreeNode
- **Purpose**: Normalized tree node rendered in the left panel.
- **Fields**:
  - `id: string`
  - `kind: 'operationGroup' | 'field' | 'type' | 'input' | 'enum' | 'directive' | 'interface' | 'schemaDocument'`
  - `label: string`
  - `typeName?: string`
  - `returnType?: string`
  - `description?: string`
  - `parentId: string | null`
  - `children: SchemaTreeNode[]`
  - `selectable: boolean`
  - `operationKind?: 'query' | 'mutation' | 'subscription'`
  - `source: 'introspection' | 'schemaDocument'`
  - `metadata?: Record<string, unknown>`
- **Validation rules**:
  - Operation groups must be root-level nodes.
  - Field nodes must include a display label and return type.
  - Uploaded schema document nodes must appear in the same tree namespace as introspection nodes.

### QueryStub
- **Purpose**: Generated editor content for a selected field.
- **Fields**:
  - `operationKind: 'query' | 'mutation' | 'subscription'`
  - `operationName: string`
  - `selectionText: string`
  - `sourceNodeId: string`
- **Validation rules**:
  - Required arguments must be represented with placeholder values.
  - Scalar-returning fields may omit a selection set.
  - Subscription stubs are valid GraphQL text but may be run only in advisory mode.

### PlaygroundLayoutState
- **Purpose**: Persistent split sizes for the panel layout.
- **Fields**:
  - `leftPaneWidthPct: number`
  - `rightTopHeightPct: number`
  - `rightBottomHeightPct: number`
  - `updatedAt: string`
- **Validation rules**:
  - Left/right panes must respect minimum widths.
  - Top/bottom panes must respect minimum heights.
  - Persisted values must be clamped to safe ranges on restore.

### QueryExecutionState
- **Purpose**: Tracks a single query run.
- **Fields**:
  - `status: 'idle' | 'loading' | 'success' | 'error' | 'advisory'`
  - `requestId: string | null`
  - `queryText: string`
  - `responseJson: unknown | null`
  - `errorText: string | null`
- **Validation rules**:
  - Only the latest in-flight request may update visible result state.
  - Run is disabled while `status === 'loading'`.

## Relationships and Flow

- `PlaygroundPageState` consumes a tree built from `SchemaTreeNode` records.
- A selected selectable `SchemaTreeNode` can produce a `QueryStub`.
- `QueryStub` becomes the current editor content.
- Running the editor content creates a `QueryExecutionState` transition.
- `PlaygroundLayoutState` is stored separately from content state so panel size persistence does not interfere with query execution.

## State Transitions

### Page lifecycle
1. `idle` → `loading tree`
2. `loading tree` → `ready` or `error`
3. `ready` → `running query`
4. `running query` → `success`, `error`, or `advisory`

### Selection lifecycle
1. `no selection` → `field selected`
2. `field selected` → `generate query enabled`
3. `field selected` → `field deselected`
4. `field deselected` → `generate query disabled`

### Layout lifecycle
1. `default split sizes`
2. `user resizes panes`
3. `persist layout preference`
4. `reload playground`
5. `restore clamped split sizes`

## Notes

- Persistence is browser-local only and contains UI preferences, not credentials.
- Subscription execution remains out of scope; only the stub-generation path is modeled as actionable.

