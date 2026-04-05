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

## 3) Verify required behavior

- Browser requests target `/graphql` (same origin) and are proxied to backend.
- No hard-coded backend hostnames are required in frontend source.
- Bootstrap route is reachable only when backend reports bootstrap allowed.
- State-changing mutations require valid auth session + CSRF.
- Organization/user/role list screens use server-driven paging/filter/sort.

## 4) Run key automated checks (recommended)

```bash
cd /home/groby/dev/isched/src/ui
pnpm test
pnpm exec playwright install chromium
pnpm e2e:bootstrap
```

```bash
cd /home/groby/dev/isched/cmake-build-debug
ctest --output-on-failure
```

## Troubleshooting

- `ECONNREFUSED` on `/graphql`: backend not running or wrong target port.
- `404` on `/graphql`: proxy not loaded; verify `--proxy-config proxy.conf.json`.
- WebSocket failures: confirm proxy has `"ws": true`.
- `CSRF_FAILED` on mutation: refresh/re-auth and retry with valid CSRF token pairing.
- `CONFLICT` on edit: refresh entity state and re-apply pending changes with current revision.

## 5) Stop local background services and clean temp data

```bash
kill "$(cat /tmp/isched_srv.pid)"
rm -f /tmp/isched_srv.pid
rm -rf "$(cat /tmp/isched_srv.data_dir)"
rm -f /tmp/isched_srv.data_dir
```


