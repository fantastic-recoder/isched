# Quickstart: Local WebUI Development

**Feature**: `004-add-isched-webui`  
**Audience**: Developers running Angular WebUI against local `isched_srv`

## Goal

Run backend and Angular WebUI locally with proxy routing for `/graphql` (HTTP + WebSocket), while using isolated backend data and the cookie-auth + CSRF model.

## Prerequisites

- Backend configured and built (for example via `python3 configure.py`)
- Node.js and `pnpm` available
- Frontend dependencies installable in `src/ui`

## 1) Start backend with isolated data dir

Default storage (without `--data-dir`) is `<DataHome>/isched` via `sago::getDataHome()`.
For repeatable local testing, use a temporary override directory:

```bash
cd /home/groby/dev/isched
TMP_DATA_DIR="$(mktemp -d)"
./cmake-build-debug/src/main/cpp/isched/isched_srv --data-dir "$TMP_DATA_DIR" 2>&1 | tee /tmp/isched_srv.log &
echo $! > /tmp/isched_srv.pid
echo "$TMP_DATA_DIR" > /tmp/isched_srv.data_dir
```

Optional connectivity check:

```bash
curl -sS http://localhost:8080/graphql \
  -H 'Content-Type: application/json' \
  -d '{"query":"query { __typename }"}'
```

## 2) Configure Angular proxy for GraphQL HTTP + WS

Ensure `src/ui/proxy.conf.json` routes `/graphql` to local backend:

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

Run the UI in standalone development mode:

```bash
cd /home/groby/dev/isched/src/ui
pnpm install
pnpm exec ng serve --proxy-config proxy.conf.json
```

Open `http://localhost:4200`.

In embedded runtime mode (without `ng serve`), the backend serves the WebUI at:

```bash
http://localhost:8080/isched
```

Canonical routing behavior in embedded mode:

- `GET /` redirects to `/isched`
- `GET /graphql` redirects to `/isched`
- GraphQL API calls remain `POST /graphql`

## 3) Verify required behavior

- Browser requests target `/graphql` (same origin) and are proxied to backend.
- No hard-coded backend hostnames are required in frontend source.
- Embedded backend WebUI entry is `/isched` with SPA fallback (for example `/isched/bootstrap`).
- Root and browser `GET /graphql` requests redirect to `/isched`.
- Missing embedded static assets return JSON `404` envelopes (and do not fall back to SPA shell).
- Valid SPA deep links (non-asset paths) return `index.html` fallback.
- Bootstrap route is reachable only when backend reports bootstrap allowed.
- State-changing mutations require valid auth session + CSRF.
- Organization/user/role list screens use server-driven paging/filter/sort.

### Embedded-serving contract probes (gap-closure)

```bash
# 1) Startup log contract: includes deterministic Admin UI startup line
grep -E "Admin UI|/isched" /tmp/isched_srv.log

# 2) Missing asset must be 404 JSON envelope (not index.html fallback)
curl -i http://localhost:8080/isched/assets/does-not-exist.js

# 3) SPA route deep link should fall back to index.html
curl -i http://localhost:8080/isched/bootstrap

# 4) Security headers on embedded responses
curl -sSI http://localhost:8080/isched | grep -Ei "content-security-policy|x-content-type-options|x-frame-options"

# 5) ETag/304 contract for embedded assets (replace with an actual served asset path if needed)
ETAG="$(curl -sSI http://localhost:8080/isched/main.js | awk -F': ' 'tolower($1)=="etag" {gsub("\r", "", $2); print $2}')"
curl -i http://localhost:8080/isched/main.js -H "If-None-Match: ${ETAG}"
```

Expected outcomes:

- Startup log contains an Admin UI status line that references embedded serving and `/isched`.
- Missing asset request returns `HTTP/1.1 404` with JSON envelope payload.
- SPA deep-link request returns HTML shell (`index.html`) for client-side routing.
- Embedded responses include CSP + `X-Content-Type-Options: nosniff` + `X-Frame-Options: DENY`.
- Asset request with matching `If-None-Match` returns `304 Not Modified` with empty body.

## 4) Run key automated checks (recommended)

```bash
cd /home/groby/dev/isched/src/ui
pnpm test
pnpm exec playwright install chromium
pnpm e2e:bootstrap
```

To run the full Playwright suite against a specific build directory (defaults to `cmake-build-debug`):

```bash
# Use the standard debug build (default)
pnpm e2e

# Use a different CMake build directory
ISCHED_BUILD_DIR=cmake-build-release pnpm e2e
ISCHED_BUILD_DIR=/path/to/custom-build pnpm e2e
```

**Environment variables for the Playwright harness:**

| Variable | Default | Description |
|---|---|---|
| `ISCHED_BUILD_DIR` | `cmake-build-debug` | CMake build directory used to locate `isched_srv` |
| `ISCHED_SERVER_PORT` | `18080` | Port the test server listens on |
| `ISCHED_EXTERNAL_SERVER` | _(unset)_ | Set to `1` to skip server launch (external harness manages it) |

```bash
cd /home/groby/dev/isched/cmake-build-debug
ctest --output-on-failure
```

## Focused validation evidence (2026-04-05)

Core WebUI validation for embedded serving and merged seed/bootstrap behavior is recorded in:

- `specs/archive/004-add-isched-webui/evidence/t022-embedded-serving-validation-2026-04-05.md`
- `docs/validation/isched-webui-004-core-validation-2026-04-05.md`

Focused command matrix used for this validation pass:

```bash
cd /home/groby/dev/isched
cmake --build ./cmake-build-debug/ --target test_webui_embedded_serving test_admin_ui test_seed_mode test_bootstrap_platform_admin

cd /home/groby/dev/isched/cmake-build-debug
ctest -R "^test_webui_embedded_serving$" --output-on-failure
ctest -R "^test_admin_ui$" --output-on-failure
ctest -R "^test_seed_mode$" --output-on-failure
ctest -R "^test_bootstrap_platform_admin$" --output-on-failure

cd /home/groby/dev/isched/src/ui
pnpm test
pnpm e2e:bootstrap
```

## Troubleshooting

- `ECONNREFUSED` on `/graphql`: backend not running or wrong target port.
- `404` on `/graphql`: proxy not loaded; verify `--proxy-config proxy.conf.json`.
- WebSocket failures: confirm proxy has `"ws": true`.
- `CSRF_FAILED` on mutation: refresh/re-auth and retry with valid CSRF token pairing.
- `CONFLICT` on edit: refresh entity state and re-apply pending changes with current revision.
- Missing Admin UI startup line in logs: verify backend binary includes embedded asset registry and startup diagnostics wiring.
- Rate-limit behavior mismatch in bootstrap/auth flows: verify bootstrap/auth-specific policy values and fallback chain (override -> feature default -> global).

## 5) Stop local background services and clean temp data

```bash
kill "$(cat /tmp/isched_srv.pid)"
rm -f /tmp/isched_srv.pid
rm -rf "$(cat /tmp/isched_srv.data_dir)"
rm -f /tmp/isched_srv.data_dir
```


