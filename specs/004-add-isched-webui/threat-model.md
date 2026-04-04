# Threat Model: 004-add-isched-webui

## Scope

This feature introduces an Angular WebUI embedded in the backend runtime and a local proxy-backed development workflow. The trust boundaries include browser runtime, reverse proxy behavior in development, GraphQL transport (`/graphql`), and multi-organization authorization context.

## Assets

- Session credentials (cookie-backed auth)
- CSRF partner token
- Organization-scoped admin mutations
- Embedded static UI assets
- GraphQL error payloads and logs

## Threats and Mitigations

- **CSRF against cookie-authenticated mutations**
  - Mitigation: mutation interceptor attaches `X-CSRF-Token`; backend must reject requests with invalid/missing token and invalid `Origin`/`Referer`.
- **Cross-organization writes from stale UI context**
  - Mitigation: explicit `OrgContextService` guard in UI and backend context mismatch rejection in resolver execution path.
- **JWT/token leakage in browser storage and logs**
  - Mitigation: no `localStorage`/`sessionStorage` persistence in auth primitives; test coverage checks for storage-free behavior in frontend.
- **Proxy misrouting and origin confusion in development**
  - Mitigation: same-origin `/graphql` client path and explicit proxy config (`src/ui/proxy.conf.json`) for HTTP and WebSocket.
- **Static asset path probing in embedded runtime**
  - Mitigation: SPA fallback behavior for route paths and explicit 404 for unknown concrete asset paths.

## Residual Risks

- Full backend CSRF enforcement across all mutation entry points is partially implemented in current codebase and requires completion in transport pipeline hardening tasks.
- Current backend GraphQL error objects still use numeric `code` values; migration to stable `extensions.code` strings remains pending.

