# Contract: Local Development Proxy

## Required Behavior

- **FR-005 is canonical**: Angular dev server proxies `/graphql` over HTTP and WebSocket; **FCR-006 references FR-005** for Angular-specific verification.
- Proxy supports HTTP GraphQL queries/mutations and WebSocket upgrade/subscription traffic.
- Frontend source uses a relative `/graphql` path and does not embed environment-specific backend hostnames.
- Proxy preserves browser-managed Secure, HttpOnly, SameSite=Strict cookie semantics; it never exposes cookie contents to Angular.
- Configure the UI dev-server origin explicitly in the backend allowed-Origin list. Origin validation remains exact even when the proxy is used.
- Never downgrade the Secure cookie for local development. Use a supported local secure-context/TLS configuration if the browser/backend combination does not attach it.
- No separate LSP route is introduced. Browser-local editor capabilities obtain schema only via GraphQL `/graphql`.

## Reference Configuration

Existing `src/ui/proxy.conf.json` maps `/graphql` to the configured backend origin and sets `ws: true`. Preserve the proxy contract, use deployment/environment configuration for target origins, and do not copy this development target into frontend application code.

## Verification

- Confirm query/mutation requests route through the proxy.
- Confirm WebSocket handshake/upgrade succeeds through the proxy and validates allowed Origin plus cookie session; `connection_init` contains no JWT.
- Confirm authenticated HTTP mutations contain the session-bound CSRF header and browser cookie.
- Confirm invalid/missing Origin is rejected and no wildcard credentialed CORS behavior bypasses the allowlist.
- Verify secure cookie behavior in supported local-development browsers without downgrading the production policy.
- On WS failure, show a diagnostic while keeping non-subscription HTTP flows available.
