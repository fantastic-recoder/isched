# Research: LSP GraphQL Angular Integration

## Decision 1: Frontend GraphQL transport architecture
- **Decision**: Centralize GraphQL client setup with HTTP+WebSocket split over `/graphql`.
- **Rationale**: Keeps transport concerns isolated, enforces GraphQL-only boundary, and simplifies feature component implementation.
- **Alternatives considered**: Per-component fetch/WebSocket calls; REST fallback adapters.

## Decision 2: Browser authentication handling
- **Decision**: Use in-memory auth context only and avoid persistent JWT storage.
- **Rationale**: Aligns with constitution security-first requirement and reduces credential persistence risk.
- **Alternatives considered**: localStorage/sessionStorage/IndexedDB token persistence.

## Decision 3: Angular architecture style
- **Decision**: Use standalone components, signal-first state, typed reactive forms, and modern template control flow.
- **Rationale**: Required by constitution and provides predictable change detection/performance characteristics.
- **Alternatives considered**: NgModule-centric feature structure; observable-first template state.

## Decision 4: Local development routing
- **Decision**: Configure Angular dev proxy for `/graphql` with WebSocket upgrade support.
- **Rationale**: Preserves environment parity and avoids hard-coded backend endpoints in client code.
- **Alternatives considered**: Direct CORS-based dev calls; hard-coded URLs.

## Decision 5: Validation strategy
- **Decision**: Layered verification with unit/component, integration, contract, security, and latency checks.
- **Rationale**: Maps directly to acceptance scenarios and measurable success criteria.
- **Alternatives considered**: Manual-only QA; backend-only tests.
