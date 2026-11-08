# Contract: Local Development Proxy

## Required Behavior
- Angular dev server proxies `/graphql` to backend origin.
- Proxy supports both HTTP requests and WebSocket upgrades.
- Frontend source code avoids hard-coded backend hostnames for dev flows.

## Verification
- Confirm query/mutation traffic routes through proxy.
- Confirm subscription WebSocket upgrade succeeds through proxy.
