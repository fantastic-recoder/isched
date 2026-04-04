# Quickstart: Local WebUI Development

**Feature**: `004-add-isched-webui`  
**Audience**: Developers running Angular WebUI against local isched backend

## Goal

Run backend and Angular WebUI locally with proxy routing for `/graphql` (HTTP + WebSocket), without hard-coded backend origins in frontend source.

## Prerequisites

- Built backend (`python3 configure.py` completed at least once)
- Node.js + pnpm installed
- Workspace dependency install completed for `src/ui`

## 1) Start backend in background

From repository root:

```bash
cd /home/groby/dev/isched
./cmake-build-debug/isched_srv > /tmp/isched_srv.log 2>&1 &
echo $! > /tmp/isched_srv.pid
```

Verify backend is reachable:

```bash
curl -sS http://localhost:8080/graphql \
  -H 'Content-Type: application/json' \
  -d '{"query":"query { version }"}'
```

## 2) Configure Angular dev proxy

Create/update `src/ui/proxy.conf.json`:

```json
{
  "/graphql": {
    "target": "http://localhost:8080",
    "secure": false,
    "changeOrigin": true,
    "ws": true,
    "logLevel": "debug"
  }
}
```

Run Angular dev server with proxy:

```bash
cd /home/groby/dev/isched/src/ui
pnpm install
pnpm exec ng serve --proxy-config proxy.conf.json
```

Open `http://localhost:4200`.

## 3) Verify GraphQL proxy behavior

- Confirm browser network requests go to `/graphql` (same origin `:4200`) and are proxied to backend `:8080`.
- Confirm no frontend code uses hard-coded backend origin for dev mode.
- Confirm authenticated mutation requests include CSRF partner token/header as required by backend contract.

## 4) Validate key flows

- First-run bootstrap appears only when backend reports bootstrap allowed.
- Organization create/edit respects platform admin and organization admin boundaries.
- User create/edit is blocked without explicit organization context.
- Role create/assign denies out-of-scope attempts with clear feedback.

## Troubleshooting

- `ECONNREFUSED` from `/graphql`: backend not running or wrong port; inspect `/tmp/isched_srv.log`.
- `404` on `/graphql`: proxy misconfigured; confirm `proxy.conf.json` path and `ng serve --proxy-config` usage.
- WebSocket subscription failures: confirm `ws: true` in proxy config.
- `CSRF_FAILED` errors on mutations: refresh session/bootstrap auth flow and retry with valid CSRF token pair.
- Immediate auth failures: verify secure cookie behavior and backend origin/proxy alignment.

## Stop local processes

```bash
kill "$(cat /tmp/isched_srv.pid)"
rm -f /tmp/isched_srv.pid
```

## Verification Evidence (2026-04-04)

Executed during implementation:

```bash
cd /home/groby/dev/isched/src/ui && pnpm test
cmake --build /home/groby/dev/isched/cmake-build-debug --target isched_graphql_tests
cd /home/groby/dev/isched/cmake-build-debug && ctest -R isched_graphql_tests --output-on-failure
```

Observed results:

- Angular tests: `11 passed, 11 total`.
- C++ build gate: `isched_graphql_tests` target built successfully.
- Backend test gate: `1/1 tests passed` for `isched_graphql_tests`.

