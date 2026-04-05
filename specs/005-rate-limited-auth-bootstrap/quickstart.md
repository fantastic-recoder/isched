# Quickstart: RATE_LIMITED + Auth Bootstrap Consistency

**Feature**: `005-rate-limited-auth-bootstrap`  
**Audience**: Developers validating deterministic auth/bootstrap behavior

## Goal

Validate lockout guidance, startup routing precedence, bootstrap transition behavior, and single-flight suppression with backend + WebUI checks.

## Prerequisites

- Repository configured and build artifacts available (for example via `python3 configure.py`)
- Node.js and `pnpm` available for `src/ui`
- Angular proxy config present in `src/ui/proxy.conf.json` for `/graphql` HTTP + WebSocket

## Validation matrix

| Requirement | Validation surface |
| --- | --- |
| FR-001, FR-002, FR-003, FR-009, SC-001, SC-002 | `ctest -R "test_rate_limiting|isched_auth_tests"`, `pnpm run test:login-lockout`, `pnpm run e2e:rate-limiting` |
| FR-004, FR-005, FR-012, SC-003 | `ctest -R "test_seed_mode"`, `pnpm run test:startup-routing` |
| FR-006, FR-007, FR-013, SC-004, SC-005 | `pnpm run test:auth-bootstrap`, `pnpm run e2e:bootstrap` |
| FR-008, FR-010, FR-011, SC-006 | `pnpm run test:auth-bootstrap`, `pnpm run e2e:auth-bootstrap`, full regression gate in step 5 |

## Focused UI command aliases

```bash
cd /home/groby/dev/isched/src/ui
pnpm run test:login-lockout
pnpm run test:startup-routing
pnpm run test:auth-bootstrap
pnpm run e2e:rate-limiting
pnpm run e2e:bootstrap
pnpm run e2e:auth-bootstrap
```

## 1) Run backend integration checks first

```bash
cd /home/groby/dev/isched/cmake-build-debug
ctest -R "test_rate_limiting|test_seed_mode|isched_auth_tests" --output-on-failure
```

## 2) Run frontend unit tests for auth/bootstrap services and routes

```bash
cd /home/groby/dev/isched/src/ui
pnpm run test:auth-bootstrap
```

## 3) Run focused E2E checks for rate-limiting and bootstrap flows

```bash
cd /home/groby/dev/isched/src/ui
pnpm run e2e:auth-bootstrap
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

