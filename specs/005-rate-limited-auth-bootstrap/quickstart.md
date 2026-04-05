# Quickstart: RATE_LIMITED + Auth Bootstrap Consistency

**Feature**: `005-rate-limited-auth-bootstrap`  
**Audience**: Developers validating deterministic auth/bootstrap behavior

## Goal

Validate lockout guidance, startup routing precedence, bootstrap transition behavior, and single-flight suppression with backend + WebUI checks.

## Prerequisites

- Repository configured and build artifacts available (for example via `python3 configure.py`)
- Node.js and `pnpm` available for `src/ui`
- Angular proxy config present in `src/ui/proxy.conf.json` for `/graphql` HTTP + WebSocket

## 1) Run backend integration checks first

```bash
cd /home/groby/dev/isched/cmake-build-debug
ctest -R "test_rate_limiting|test_seed_mode|isched_auth_tests" --output-on-failure
```

## 2) Run frontend unit tests for auth/bootstrap services and routes

```bash
cd /home/groby/dev/isched/src/ui
pnpm test -- --runInBand auth.service.spec.ts graphql.service.spec.ts app.spec.ts
```

## 3) Run focused E2E checks for rate-limiting and bootstrap flows

```bash
cd /home/groby/dev/isched/src/ui
pnpm e2e --grep "rate|bootstrap|login"
```

## 4) Manual verification matrix (if needed)

- Seed mode active + no valid session -> app lands on bootstrap.
- Seed mode active + valid session -> app still lands on bootstrap first.
- Seed mode inactive + no valid session -> app lands on sign-in.
- Seed mode inactive + valid session -> protected route access without transient misrouting.
- Repeated submit clicks during login/bootstrap pending -> only one network request in flight per flow.
- `RATE_LIMITED` with and without `retryAfterMs` -> deterministic alert text always shown.

## 5) Full regression before merge

```bash
cd /home/groby/dev/isched/cmake-build-debug
ctest --output-on-failure

cd /home/groby/dev/isched/src/ui
pnpm test
pnpm e2e
```

## Troubleshooting

- If startup routing is flaky, inspect first boot GraphQL calls (`systemState`, `currentUser`) in browser devtools.
- If lockout guidance is missing, verify GraphQL errors contain `extensions.code = RATE_LIMITED`.
- If duplicate requests appear, inspect pending-state guard logic in auth/bootstrap submit handlers.

