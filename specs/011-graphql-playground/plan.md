# Implementation Plan: GraphQL Playground

**Branch**: `011-graphql-playground` | **Date**: 2026-05-23 | **Spec**: `/home/groby/dev/isched/specs/011-graphql-playground/spec.md`
**Input**: Feature specification from `/specs/011-graphql-playground/spec.md`

## Summary

Add an authenticated `/playground` page to the Angular UI that lets users browse the full GraphQL introspection tree, inspect merged uploaded schema documents, generate starter operations, run queries against `/graphql`, and resize/persist the workspace layout. The implementation stays frontend-first, uses CodeMirror 6 for the editor, keeps subscription support generate-only, and reuses the existing GraphQL transport and authentication patterns already present in the app.

## Architecture Overview

- Frontend-first Angular feature added as a new route/page under the authenticated shell.
- The page composes a left schema browser and a right workspace split into editor/result panels.
- A reusable resizable split component handles both panel axes and emits normalized layout state.
- A tree-normalization service transforms introspection plus uploaded schema documents into one navigable model.
- A query service reuses the existing GraphQL transport for execution; no alternate API surface is introduced.
- A small persistence service stores only browser layout preferences and restores them on startup.

## Component Breakdown

- **PlaygroundPage**: route entry point, orchestration, selection-to-editor flow, run-state coordination.
- **SchemaTreeComponent**: renders the full introspection tree and merged schema-document nodes.
- **QueryEditorComponent**: wraps CodeMirror 6 and exposes editor content changes.
- **ResultPanelComponent**: renders loading, success, error, and subscription-advisory states.
- **ResizableSplitComponent**: reusable drag splitter for left/right and top/bottom layouts.
- **PlaygroundIntrospectionService**: loads and normalizes schema discovery data.
- **PlaygroundQueryService**: sends ad-hoc GraphQL operations to `/graphql` and maps responses/errors.
- **PlaygroundLayoutService**: persists and restores panel sizes across sessions.

## Backend / Frontend Touchpoints

- **Frontend route**: `/playground` guarded by the existing authenticated routing model.
- **Backend data source**: existing `/graphql` introspection and `schemaDocuments` response shape.
- **Query execution**: HTTP POST to `/graphql` for queries and mutations only.
- **Subscriptions**: frontend generates stubs but does not execute them in this iteration.
- **Schema documents**: merged into the main tree as named nodes instead of a separate group.
- **Dev environment**: Angular dev-server proxy remains the transport path for local `/graphql` access.

## Data Flow

1. User opens `/playground`; auth guard resolves access.
2. Playground loads introspection and schema-document data through `/graphql`.
3. Services normalize the payload into a single tree model and signal-backed view state.
4. Selecting a query/mutation/subscription node enables Generate Query.
5. Generate Query inserts a stub into CodeMirror 6, including placeholder arguments.
6. Run sends the current editor content to `/graphql`.
7. Result panel shows loading, formatted JSON, error state, or subscription advisory.
8. Splitter changes update persisted layout state and are restored on next visit.

## Test Strategy

- Unit tests cover tree normalization, node selection, stub generation, run-state transitions, and layout persistence.
- Component tests verify CodeMirror integration, disabled/enabled button states, and result-panel rendering paths.
- Playwright e2e covers auth-gated access, tree rendering, generated-query execution, and resize persistence.
- Regression checks should confirm subscription stubs remain generate-only and that no REST calls are introduced.

## Risks

- Large introspection trees can be expensive to render; mitigate with normalized data structures and careful change detection.
- Stub generation must handle required arguments and nested field selections correctly; cover representative schema shapes in unit tests.
- Persistent layout state can drift into invalid sizes after viewport changes; clamp on restore and on drag end.
- Subscription generate-only behavior may confuse users; show an explicit advisory message in the result panel.
- Merged uploaded schemas increase tree density; keep labels concise and enforce tree virtualization only if needed later.

## Technical Context

**Language/Version**: TypeScript with Angular 21 on the frontend; existing C++23 backend preserved  
**Primary Dependencies**: Angular standalone APIs, RxJS for request streams, CodeMirror 6 GraphQL editor packages, existing `GraphQLService` transport, Tailwind CSS 3.x + DaisyUI 4.x  
**Storage**: Browser-persistent layout preferences only; no new server-side storage  
**Testing**: Angular unit tests (`pnpm test:ci`) and Playwright e2e (`pnpm e2e`)  
**Target Platform**: Linux development environment, authenticated web browser clients  
**Project Type**: Web application  
**Performance Goals**: Fast first render of the tree, smooth drag resizing, responsive selection/generation actions, and no visible layout jank while loading or executing queries  
**Constraints**: GraphQL-only backend access, no persistent JWT storage, strict Angular template/state rules, persistent panel sizes across sessions, subscription execution deferred  
**Scale/Scope**: One new authenticated page with a large, normalized schema tree derived from full introspection plus merged schema documents

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] GraphQL-only external interface preserved (`/graphql` HTTP/WebSocket only)
- [x] Language and build baseline preserved (C++23, Conan-managed dependencies, CMake + Ninja)
- [x] WebUI changes (if any) follow Angular standards: signal-first state, standalone APIs, `@if/@for/@switch`, typed reactive forms, strict TypeScript, zoneless/`OnPush`-compatible patterns, and no async-pipe-driven template state for app-owned UI state
- [x] Frontend API calls (if any) use GraphQL `/graphql` only; no REST endpoints or alternate transports introduced
- [x] Browser JWT handling (if any) avoids persistent token storage (`localStorage`/`sessionStorage`/IndexedDB) and documents secure cookie or equivalent controls
- [x] Local Angular development (if any) uses a proxy for `/graphql` (HTTP + WebSocket) with no hard-coded backend hostnames in client source
- [x] Test plan proves required coverage before each user story is marked complete
- [x] Security-sensitive changes include both feature-scoped and project-level threat-model updates
- [x] New code follows Clean Code Principles: functions are small and focused, polymorphism preferred over complex conditional chains, and any hot-path abstraction trade-offs are documented with profiling evidence

## Project Structure

### Documentation (this feature)

```text
specs/011-graphql-playground/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
└── contracts/
    └── playground-contract.md
```

### Source Code (repository root)

```text
src/ui/src/app/
├── pages/
│   └── playground/
│       ├── playground.ts
│       ├── playground.html
│       ├── playground.scss
│       └── playground.spec.ts
├── components/
│   └── playground/
│       ├── schema-tree/
│       ├── query-editor/
│       ├── result-panel/
│       └── resizable-split/
└── services/
    ├── playground-introspection.service.ts
    ├── playground-query.service.ts
    └── playground-layout.service.ts

src/main/cpp/isched/backend/
└── isched_GqlExecutor.cpp  # verify/extend introspection and schemaDocuments exposure only if required
```

**Structure Decision**: Implement the feature as a new Angular page under `src/ui/src/app/pages/playground/` with supporting standalone components and small services under the existing `components/` and `services/` folders. Keep backend changes minimal and limited to existing GraphQL introspection/schema-document plumbing if the current schema contract needs adjustment.

## Implementation Phases

1. **Route and shell integration**: add `/playground` routing, guard access, and place the entry in authenticated navigation.
2. **Schema discovery and tree model**: load full introspection plus `schemaDocuments`, normalize into a merged tree, and render selection state.
3. **Editor and execution flow**: add CodeMirror 6, implement query generation, run queries, and render result states.
4. **Resizable layout and persistence**: wire both splitters, clamp sizes, and persist/restore layout preferences.
5. **Hardening and tests**: complete unit coverage, Playwright coverage, and cleanup for accessibility/performance.

## Complexity Tracking

No constitutional violations identified; no exception table required.
