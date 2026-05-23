# Tasks: GraphQL Playground (SP-011)

**Branch**: `011-graphql-playground`  
**Plan**: `plan.md`  
**Spec**: `spec.md`  
**Generated**: 2026-05-23

---

## Dependency Order

```
T-01 → T-02 → T-03 → T-04 → T-05 → T-10
                        ↓
T-06 → T-07 → T-08 → T-09 → T-10
```

- T-01 through T-05 cover routing, shell, and services (no UI yet).
- T-06 through T-09 cover components (require services from T-03/T-04).
- T-10 wires everything together and is the final integration + hardening pass.

---

## Phase 1 — Route & Shell Integration

### T-01 — Add `/playground` route with auth guard

**Spec refs**: FR-001, FR-002, SC-001  
**Depends on**: nothing  
**Output**: A new lazy-loaded route entry in `app.routes.ts` guarded by the existing `authGuard`, pointing to a stub `PlaygroundPage` component.

**Steps**:
1. Create `src/ui/src/app/pages/playground/playground.ts` as a minimal standalone component (empty shell).
2. Create `src/ui/src/app/pages/playground/playground.html` (placeholder heading).
3. Create `src/ui/src/app/pages/playground/playground.scss` (empty).
4. Add to `app.routes.ts`:
   ```ts
   {
     path: 'playground',
     canActivate: [authGuard],
     loadComponent: () =>
       import('./pages/playground/playground').then((m) => m.PlaygroundPage),
   },
   ```
5. Add a "Playground" link to the main navigation bar component (`nav` or equivalent).

**Acceptance**:
- `ng build` compiles without errors.
- Navigating to `/playground` while unauthenticated redirects to `/login` (automated unit test on `authGuard`).
- Navigating to `/playground` while authenticated loads the stub page.

**Tests**: Add/extend `auth.guard.spec.ts` to cover the `/playground` guard path; add a basic `playground.spec.ts` that verifies the component mounts.

---

### T-02 — Install CodeMirror 6 and GraphQL language support packages

**Spec refs**: FR-009  
**Depends on**: T-01 (need UI project structure confirmed working)  
**Output**: `codemirror`, `@codemirror/lang-graphql`, and supporting theme/state packages added to `src/ui/package.json`.

**Steps**:
1. Run from `src/ui/`:
   ```bash
   pnpm add @codemirror/view @codemirror/state @codemirror/basic-setup codemirror @codemirror/lang-graphql
   ```
2. Verify `pnpm install` resolves without conflicts.
3. Confirm `ng build` still passes after the dependency addition.

**Acceptance**:
- All CodeMirror 6 packages appear in `package.json` and `pnpm-lock.yaml`.
- `ng build` produces no new type errors.

---

## Phase 2 — Services

### T-03 — Implement `PlaygroundIntrospectionService`

**Spec refs**: FR-004, FR-005, FR-007  
**Depends on**: T-01  
**Output**: `src/ui/src/app/services/playground-introspection.service.ts` that fetches full introspection + `schemaDocuments` and returns a normalized `SchemaTreeNode[]` signal.

**Steps**:
1. Define `SchemaTreeNode` interface/type in `src/ui/src/app/services/playground-introspection.models.ts`:
   ```ts
   export type NodeKind =
     | 'operationGroup' | 'field' | 'objectType' | 'inputType'
     | 'enumType' | 'directive' | 'interfaceType' | 'scalarType'
     | 'schemaDocument';

   export interface SchemaTreeNode {
     id: string;
     kind: NodeKind;
     name: string;
     typeName?: string;   // return type for field nodes
     args?: IntrospectionInputValue[];
     children: SchemaTreeNode[];
     isSelectable: boolean;  // true for query/mutation/subscription fields
   }
   ```
2. Implement `PlaygroundIntrospectionService` as a standalone `@Injectable({ providedIn: 'root' })`:
   - Expose `readonly treeNodes: Signal<SchemaTreeNode[]>` (derived from a `WritableSignal` or `toSignal`).
   - Expose `readonly loading: Signal<boolean>` and `readonly error: Signal<string | null>`.
   - Implement `load()` method that:
     a. Executes the full introspection query against `/graphql` via the existing `GraphQLService`.
     b. Executes `query { schemaDocuments { id name content } }`.
     c. Normalizes both results into a single `SchemaTreeNode[]` tree:
        - Top-level groups: `Queries`, `Mutations`, `Subscriptions`, `Types`, `Inputs`, `Enums`, `Directives`, `Interfaces`.
        - Schema document nodes merged into a `SchemaDocuments` group or as siblings to `Types`.
3. Handle introspection failure: set `error` signal to a user-readable string; return empty `treeNodes`.

**Acceptance**:
- Unit tests with a mocked `GraphQLService` verify:
  - Tree contains correct operation groups for a sample introspection payload.
  - Schema document nodes appear merged into the tree.
  - `loading` is `true` during fetch and `false` after.
  - `error` is populated when introspection fails.
- `ng test` passes with ≥ 80% branch coverage on this service.

**Tests**: `src/ui/src/app/services/playground-introspection.service.spec.ts`

---

### T-04 — Implement `PlaygroundQueryService`

**Spec refs**: FR-010, FR-011, FR-012, FR-013, FR-021  
**Depends on**: T-01  
**Output**: `src/ui/src/app/services/playground-query.service.ts` that executes a raw GraphQL string query and returns structured result state.

**Steps**:
1. Implement `PlaygroundQueryService` as `@Injectable({ providedIn: 'root' })`:
   - Method signature: `execute(queryText: string): Observable<PlaygroundQueryResult>` where:
     ```ts
     export interface PlaygroundQueryResult {
       loading: boolean;
       data?: unknown;
       errors?: string[];
       isSubscriptionAdvisory?: boolean;
     }
     ```
   - Detect `subscription` operations in the query text; if found, return `{ loading: false, isSubscriptionAdvisory: true }` immediately without making a network request.
   - For queries and mutations, POST to `/graphql` via `HttpClient` (or the existing `GraphQLService`) and map the response to `PlaygroundQueryResult`.
   - Emit `{ loading: true }` first, then the final result.
2. Ensure no bearer tokens or JWT data are stored in browser storage inside this service.

**Acceptance**:
- Unit tests verify:
  - Subscription strings produce `isSubscriptionAdvisory: true` without any HTTP call.
  - A successful mock HTTP response maps to `data` populated.
  - A GraphQL `errors` field in the response maps to `errors` populated.
  - An HTTP error produces `errors` with a human-readable fallback.
- `ng test` passes with ≥ 80% branch coverage.

**Tests**: `src/ui/src/app/services/playground-query.service.spec.ts`

---

### T-05 — Implement `PlaygroundLayoutService`

**Spec refs**: FR-017, FR-018, FR-019, FR-020  
**Depends on**: T-01  
**Output**: `src/ui/src/app/services/playground-layout.service.ts` that persists and restores left/right and top/bottom panel sizes.

**Steps**:
1. Define `PlaygroundLayout` interface:
   ```ts
   export interface PlaygroundLayout {
     leftWidthPct: number;   // 0–100, clamped to [20, 80]
     topHeightPct: number;   // 0–100, clamped to [15, 85]
   }
   export const DEFAULT_LAYOUT: PlaygroundLayout = { leftWidthPct: 30, topHeightPct: 50 };
   ```
2. Implement `PlaygroundLayoutService`:
   - On construction: read layout from `localStorage` key `isched.playground.layout`; fall back to `DEFAULT_LAYOUT` if absent or invalid.
   - Expose `readonly layout: Signal<PlaygroundLayout>`.
   - Expose `setLeftWidth(pct: number)` and `setTopHeight(pct: number)` that clamp, update the signal, and persist to `localStorage`.
   - Clamp values: `leftWidthPct` in [20, 80]; `topHeightPct` in [15, 85].
3. **Note on storage**: This stores only layout *preference* data, not JWTs or session data; use of `localStorage` is acceptable here per the spec assumption.

**Acceptance**:
- Unit tests verify:
  - Default layout is returned when `localStorage` is empty.
  - Values are clamped at min/max boundaries.
  - Stored values are restored on service construction in tests that pre-populate `localStorage`.
- `ng test` passes with ≥ 80% branch coverage.

**Tests**: `src/ui/src/app/services/playground-layout.service.spec.ts`

---

## Phase 3 — Components

### T-06 — Implement `ResizableSplitComponent`

**Spec refs**: FR-017, FR-018, FR-019  
**Depends on**: T-05  
**Output**: A reusable standalone Angular component `src/ui/src/app/components/playground/resizable-split/` that renders two content slots separated by a draggable divider.

**Files**:
- `resizable-split.ts`
- `resizable-split.html`
- `resizable-split.scss`
- `resizable-split.spec.ts`

**Steps**:
1. Implement `ResizableSplitComponent` with:
   - `@Input() direction: 'horizontal' | 'vertical'` (vertical = L/R, horizontal = T/B).
   - `@Input() initialSizePct = 50`.
   - `@Output() sizeChange = new EventEmitter<number>()` (emits the first-panel percentage after drag end).
   - Host layout: use CSS flex/grid so the two `<ng-content select="[slot=first]">` and `<ng-content select="[slot=second]">` slots fill the available space proportionally.
   - Drag handle: a thin `<div class="divider">` with `mousedown` listener; track `mousemove`/`mouseup` on `document` while dragging (clean up on destroy).
   - Enforce min sizes: translate the min-px values from spec (200px for L/R, 80px for T/B) as proportion guards — reject a new size if the resulting panel would be smaller than the minimum.
2. Use `OnPush` change detection and signal-based internal drag state.
3. Style the divider with DaisyUI / Tailwind utilities. Cursor changes to `col-resize` / `row-resize` on hover.

**Acceptance**:
- Unit tests using `TestBed` verify that:
  - `sizeChange` emits the correct value after simulated mousedown + move + mouseup.
  - Values below the minimum are clamped before emission.
  - Host element layout is correctly set by the `direction` input.
- `ng test` passes with ≥ 80% branch coverage.

---

### T-07 — Implement `SchemaTreeComponent`

**Spec refs**: FR-003, FR-005, FR-006, FR-007, FR-014  
**Depends on**: T-03  
**Output**: `src/ui/src/app/components/playground/schema-tree/`

**Files**:
- `schema-tree.ts`
- `schema-tree.html`
- `schema-tree.scss`
- `schema-tree.spec.ts`

**Steps**:
1. Implement `SchemaTreeComponent`:
   - `@Input()` accepts `nodes: SchemaTreeNode[]` and `loading: boolean` and `error: string | null`.
   - `@Output() nodeSelected = new EventEmitter<SchemaTreeNode | null>()`.
   - Renders the tree recursively using `@for` and `@if` (no `ngFor`/`*ngIf`).
   - Each node shows: expand/collapse chevron (if it has children), a type-appropriate icon, `name`, and `typeName` for field nodes.
   - Clicking a `isSelectable` node emits `nodeSelected` and visually highlights it.
   - Clicking a group node toggles collapse.
   - Generate Query button at the bottom of the panel: shown as a DaisyUI `btn btn-primary btn-sm`, disabled when no `isSelectable` node is selected.
   - `@Output() generateQuery = new EventEmitter<SchemaTreeNode>()` emitted from this button.
2. Show a loading skeleton (DaisyUI `skeleton` class or Tailwind pulse) when `loading` is true.
3. Show an error alert (DaisyUI `alert alert-error`) when `error` is non-null.
4. Use `OnPush`.

**Acceptance**:
- Unit tests verify:
  - Tree renders correct operation group labels from mocked `SchemaTreeNode[]`.
  - Clicking `isSelectable` node emits `nodeSelected`.
  - Generate Query button is disabled when nothing is selected (`nodeSelected` never emitted or `null` emitted).
  - Generate Query button is enabled when a selectable node is selected.
  - Clicking Generate Query emits `generateQuery`.
  - Loading skeleton shown when `loading = true`.
  - Error alert shown when `error` is set.

---

### T-08 — Implement `QueryEditorComponent`

**Spec refs**: FR-009, FR-015  
**Depends on**: T-02  
**Output**: `src/ui/src/app/components/playground/query-editor/`

**Files**:
- `query-editor.ts`
- `query-editor.html`
- `query-editor.scss`
- `query-editor.spec.ts`

**Steps**:
1. Implement `QueryEditorComponent`:
   - Initializes a CodeMirror 6 `EditorView` in `ngAfterViewInit` inside a `<div #editorHost>`.
   - Extensions: `basicSetup`, `graphql()` language pack, a light/dark theme compatible with DaisyUI.
   - `@Input()` set`initialContent(value: string)` — replaces the editor content programmatically (used by Generate Query flow).
   - `@Output() contentChange = new EventEmitter<string>()` debounced at ~300ms.
   - Exposes `getContent(): string` public method for the parent page to read on Run.
   - Cleans up the `EditorView` in `ngOnDestroy`.
2. Style the editor container with a DaisyUI card-like border styled via Tailwind utilities.

**Acceptance**:
- Unit test (using `TestBed` + `fakeAsync`) verifies:
  - Setting `initialContent` replaces the editor document.
  - `contentChange` emits after content is modified.
- Visual smoke: `ng build` produces no type errors.

---

### T-09 — Implement `ResultPanelComponent`

**Spec refs**: FR-011, FR-012, FR-013, FR-021  
**Depends on**: T-04  
**Output**: `src/ui/src/app/components/playground/result-panel/`

**Files**:
- `result-panel.ts`
- `result-panel.html`
- `result-panel.scss`
- `result-panel.spec.ts`

**Steps**:
1. Implement `ResultPanelComponent`:
   - `@Input() result: PlaygroundQueryResult | null`.
   - States rendered via `@if / @switch`:
     - `null` → empty/idle placeholder (faint "Run a query to see results" message).
     - `loading: true` → DaisyUI `loading loading-spinner` centered.
     - `isSubscriptionAdvisory: true` → DaisyUI `alert alert-info` with "Subscriptions are not yet supported in the playground."
     - `errors` populated → DaisyUI `alert alert-error` listing each error.
     - `data` populated → preformatted JSON in a scrollable `<pre>` block with `JSON.stringify(data, null, 2)`.
   - For large payloads (> 100 kB), show a truncated preview with an "Expand" button.
2. Use `OnPush`.

**Acceptance**:
- Unit tests verify each of the five state branches renders the correct element and/or text.
- `ng test` passes with ≥ 80% branch coverage.

---

## Phase 4 — Playground Page Integration

### T-10 — Assemble `PlaygroundPage` — wire all components and services

**Spec refs**: All FR-* and FCR-*  
**Depends on**: T-03, T-04, T-05, T-06, T-07, T-08, T-09  
**Output**: Fully implemented `PlaygroundPage` with all panels wired.

**Steps**:
1. Replace the stub `PlaygroundPage` with the full implementation:
   - Inject `PlaygroundIntrospectionService`, `PlaygroundQueryService`, `PlaygroundLayoutService`.
   - Call `introspectionService.load()` in constructor or `ngOnInit`.
   - Template structure:
     ```
     <div class="playground-shell">
       <resizable-split direction="vertical" (sizeChange)="onLeftResize($event)">
         <div slot="first">
           <schema-tree [nodes]="..." (generateQuery)="onGenerateQuery($event)" />
         </div>
         <div slot="second">
           <resizable-split direction="horizontal" (sizeChange)="onTopResize($event)">
             <div slot="first"><query-editor #editor /></div>
             <div slot="second"><result-panel [result]="queryResult()" /></div>
           </resizable-split>
         </div>
       </resizable-split>
     </div>
     ```
   - Signal-backed state: `selectedNode`, `queryResult`, `isRunning`.
   - `onGenerateQuery(node)`: call a `QueryStubGenerator` helper (see step 2) → set `editor.initialContent(stub)`.
   - Run button: in the `query-editor` section or as a floating button above the editor — calls `execute()` which calls `playgroundQueryService.execute(editorContent)`.
   - Disable Run button while `isRunning()` is `true`.
   - On resize events: call `layoutService.setLeftWidth()` and `layoutService.setTopHeight()`.
   - On init, apply persisted layout percentages to the `ResizableSplitComponent` inputs.

2. Implement `QueryStubGenerator` as a pure function/utility in `src/ui/src/app/pages/playground/query-stub-generator.ts`:
   - `generateStub(node: SchemaTreeNode): string`
   - Produces a minimal valid GraphQL operation string from a field node:
     - Wraps in `query / mutation / subscription { ... }` based on parent group.
     - Adds arguments with placeholder values typed by `argType`: `String` → `"..."`, `Int` / `Float` → `0`, `Boolean` → `false`, `ID` → `"..."`, custom objects → `{}`.
     - Adds a `{ id }` sub-selection for object return types (safe default).
   - Pure function with no Angular dependencies — easy to unit test.

3. Apply `OnPush` to `PlaygroundPage`.

4. Ensure the page uses `templateUrl: 'playground.html'` and `styleUrl: 'playground.scss'` (never inline).

**Acceptance**:
- Unit tests on `QueryStubGenerator` cover:
  - Query field with no arguments.
  - Query field with required `ID!`, `String!`, `Int!`, `Boolean!` arguments.
  - Mutation field with nested input type argument.
  - Subscription field generates `subscription { ... }` stub.
- `PlaygroundPage` spec verifies:
  - `load()` is called on init (spy on service).
  - `onGenerateQuery` sets editor content through the service.
  - Run button is disabled during `isRunning` state.
  - Layout percentages are read from `PlaygroundLayoutService` on init.
- `ng build` and `ng test` both pass without errors.

---

## Phase 5 — Tests & Hardening

### T-11 — Complete Angular unit test coverage

**Spec refs**: SC-004, SC-007, US-5  
**Depends on**: T-10  
**Output**: All playground component and service specs achieving ≥ 80% branch coverage.

**Steps**:
1. Run `pnpm test:ci` and collect coverage report.
2. Identify uncovered branches in:
   - `PlaygroundIntrospectionService` (error paths, empty `schemaDocuments`).
   - `PlaygroundQueryService` (HTTP error, subscription detection edge cases).
   - `PlaygroundLayoutService` (clamping at both ends, `localStorage` parse error).
   - `ResizableSplitComponent` (drag to min/max, direction switch).
   - `SchemaTreeComponent` (deep nesting, expand/collapse cycles).
   - `ResultPanelComponent` (large payload truncation).
   - `QueryStubGenerator` (scalar return type, missing args, nested input).
3. Add tests until ≥ 80% branch coverage is achieved on all listed files.
4. Verify `ng test` exits with no failing specs.

**Acceptance**:
- `pnpm test:ci` exits 0 with ≥ 80% branch coverage reported for all playground source files.

---

### T-12 — Playwright e2e tests for playground

**Spec refs**: SC-003, SC-005, US-5  
**Depends on**: T-10, T-11  
**Output**: A new Playwright test file `tests/e2e/playground.spec.ts` covering auth-gate, tree rendering, generate-query, query execution, and panel resize.

**Steps**:
1. Create `tests/e2e/playground.spec.ts`:

   **Test: unauthenticated redirect** (covers SC-001)
   - Navigate to `/isched/playground` without login.
   - Expect redirect to `/isched/login`.

   **Test: tree renders operation groups** (covers SC-002)
   - Log in with a valid admin account.
   - Navigate to `/isched/playground`.
   - Assert the schema tree contains at least `Queries` and `Mutations` group nodes.

   **Test: generate and run `health` query** (covers SC-003)
   - Log in, navigate to playground.
   - Click on the `health` field node in the tree.
   - Click the "Generate Query" button.
   - Assert the editor contains `query` and `health`.
   - Click the "Run" button.
   - Assert the result panel shows JSON with a `health` key.

   **Test: panel resize persists** (covers SC-005, FR-020)
   - Log in, navigate to playground.
   - Drag the vertical divider 80px to the right.
   - Navigate away to `/isched/dashboard`.
   - Navigate back to `/isched/playground`.
   - Assert the left panel width is wider than the default (persisted layout applied).

   **Test: subscription advisory** (covers FR-021)
   - If a `Subscriptions` group exists, select any subscription field, generate stub, click Run.
   - Assert result panel shows the advisory message (does not show spinner indefinitely).

2. Reuse the test server startup helper from existing e2e infrastructure (`tools/run_e2e.py` or equivalent).

**Acceptance**:
- All five test scenarios pass with `pnpm e2e`.
- No JavaScript console errors or layout overflow errors logged during resize test.

---

### T-13 — Accessibility, performance, and final cleanup

**Spec refs**: SC-005, non-functional quality  
**Depends on**: T-12  
**Output**: Minor accessibility fixes and performance considerations documented.

**Steps**:
1. Add `aria-expanded` and `role="tree"` / `role="treeitem"` attributes to the schema tree.
2. Add `aria-label` to Run/Generate buttons.
3. Verify no `console.error` entries appear during normal playground use in e2e tests.
4. Verify that navigating away from `/playground` while a query is in-flight cancels the pending request (check `takeUntilDestroyed` or equivalent is used in `PlaygroundQueryService`).
5. Confirm no unnecessary change detection cycles appear (OnPush on all playground components).
6. Run `ng build --configuration=production` to confirm the bundle builds cleanly.
7. Commit all changes.

**Acceptance**:
- `ng build --configuration=production` exits 0.
- `pnpm test:ci` exits 0.
- `pnpm e2e` exits 0.
- `ctest --output-on-failure` in `cmake-build-debug/` exits 0.
- Commit message: `feat(playground): implement GraphQL playground (SP-011)`.

---

## Summary Table

| Task | Phase | Description | Depends on |
|------|-------|-------------|------------|
| T-01 | Shell | `/playground` route + auth guard + nav link | — |
| T-02 | Shell | Install CodeMirror 6 packages | T-01 |
| T-03 | Services | `PlaygroundIntrospectionService` | T-01 |
| T-04 | Services | `PlaygroundQueryService` | T-01 |
| T-05 | Services | `PlaygroundLayoutService` | T-01 |
| T-06 | Components | `ResizableSplitComponent` | T-05 |
| T-07 | Components | `SchemaTreeComponent` | T-03 |
| T-08 | Components | `QueryEditorComponent` | T-02 |
| T-09 | Components | `ResultPanelComponent` | T-04 |
| T-10 | Integration | Assemble `PlaygroundPage` + `QueryStubGenerator` | T-03–T-09 |
| T-11 | Testing | Complete Angular unit coverage (≥ 80%) | T-10 |
| T-12 | Testing | Playwright e2e tests | T-10–T-11 |
| T-13 | Hardening | A11y, performance, final build & commit | T-12 |

