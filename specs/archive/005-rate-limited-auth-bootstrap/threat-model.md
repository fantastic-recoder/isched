# Threat Model: 005-rate-limited-auth-bootstrap

**Last updated**: 2026-04-05

## Scope

This feature updates authentication lockout signaling and startup/bootstrap route behavior in the WebUI. Trust boundaries include browser UI state, GraphQL error metadata, route guards, and backend session/bootstrap state checks.

## Assets

- Authentication session state (server-managed, cookie-backed)
- Lockout/error metadata (`extensions.code`, `retryAfterMs`)
- Bootstrap eligibility state (`seedModeActive`)
- Guard-controlled route access decisions
- User-facing security guidance messages

## Threats, Mitigations, and Evidence

| Threat | Final mitigation state | Evidence |
| --- | --- | --- |
| Ambiguous lockout responses causing repeated brute-force retries | `RATE_LIMITED` lockout signaling is normalized, with metadata-aware and fallback retry guidance paths in UI mapping. | `src/test/cpp/integration/test_rate_limiting.cpp`, `src/ui/src/app/services/auth.service.spec.ts`, `src/ui/src/app/services/graphql.service.spec.ts`, `src/ui/src/app/pages/login/login.spec.ts` |
| Session state drift between app init and guarded navigation | Startup route resolution + one-time guard revalidation enforce deterministic redirect to sign-in when session becomes invalid. | `src/test/cpp/integration/test_seed_mode.cpp`, `src/ui/src/app/app.spec.ts`, `src/ui/src/app/guards/auth.guard.spec.ts` |
| Duplicate auth/bootstrap submissions creating race conditions | Single-flight suppression blocks duplicate login/bootstrap submits while original request is pending. | `src/ui/src/app/pages/login/login.spec.ts`, `src/ui/src/app/pages/bootstrap/bootstrap.page.spec.ts`, `src/ui/e2e/bootstrap.spec.ts` |
| Bootstrap page exposed after bootstrap is no longer allowed | Bootstrap-unavailable flow redirects to sign-in with explicit recovery notice. | `src/test/cpp/integration/test_bootstrap_platform_admin.cpp`, `src/ui/src/app/pages/bootstrap/bootstrap.page.spec.ts`, `src/ui/e2e/bootstrap.spec.ts` |
| Credential/token exposure in browser storage | Access/session indicators remain in-memory only; no `localStorage`/`sessionStorage`/IndexedDB token persistence introduced. | `src/ui/src/app/services/auth.service.ts`, `src/ui/src/app/interceptors/auth.interceptor.spec.ts`, `docs/security-threat-model.md` |

## Validation Run Evidence (2026-04-05)

- `ctest --output-on-failure` (full backend gate): PASS (39/39)
- `ctest -R 'test_rate_limiting|test_seed_mode|isched_auth_tests' --output-on-failure`: PASS
- `pnpm run test:login-lockout`: PASS
- `pnpm run test:startup-routing`: PASS
- `pnpm run test:auth-bootstrap`: PASS
- `pnpm test`: PASS (16 suites / 86 tests)
- `pnpm run e2e:bootstrap`: PASS (3/3)
- `pnpm run e2e:rate-limiting`: PASS (4/4)
- `pnpm run e2e:auth-bootstrap`: PASS (7/7)
- `pnpm e2e`: PASS (7/7)

## Residual Risks

- Guard revalidation correctness remains dependent on reliable and low-latency `currentUser` responses under degraded network conditions.

