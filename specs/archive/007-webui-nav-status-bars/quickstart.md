# Quickstart: WebUI Navigation + Status Bars

**Feature**: `007-webui-nav-status-bars`  
**Audience**: Developers validating authenticated shell navigation/status behavior

## Goal

Validate top navigation visibility and routing, bottom status bar digest + user identity behavior, and regression protection via Angular unit tests and Playwright smoke checks.

## Prerequisites

- Repository configured/buildable (`python3 configure.py`)
- Frontend dependencies installed for `src/ui` (via `pnpm install` if needed)
- Angular proxy available at `src/ui/proxy.conf.json` for `/graphql` HTTP + WebSocket forwarding

## Validation Matrix

| Requirement | Validation Surface |
| --- | --- |
| FR-001, FR-002, FR-003, FR-008, FR-009 | Shell unit tests + Playwright smoke nav checks |
| FR-004, FR-005, FR-007 | Shell status-bar unit tests + Playwright smoke status checks |
| FR-006 | Unit tests for digest loading/success/failure wording and representative organization-user flow |
| FR-010 | `pnpm run test` and `pnpm run test:shell` include shell rendering + digest transition suites |
| FR-011 | `pnpm run e2e` and `pnpm run e2e:shell` validate global shell behavior in a running app |
| FCR-006, SC-005 | `pnpm run e2e:dev-proxy` validates Angular dev-server `/graphql` proxy forwarding |
| SC-001, SC-003, SC-004 | Combined unit + smoke suite execution in CI-like run |

## 1) Run focused frontend unit tests (shell + tracked operations)

```bash
cd /home/groby/dev/isched/src/ui
pnpm run test:shell
```

## 2) Run complete frontend unit gate

```bash
cd /home/groby/dev/isched/src/ui
pnpm run test
```

## 3) Run focused Playwright smoke check for shell behavior

```bash
cd /home/groby/dev/isched/src/ui
pnpm run e2e:shell
```

## 4) Run full Playwright smoke/regression suite

```bash
cd /home/groby/dev/isched/src/ui
pnpm run e2e
```

## 5) Run Angular dev-server proxy health check

```bash
cd /home/groby/dev/isched/src/ui
pnpm run e2e:dev-proxy
```

## 6) Run repository regression gate before merge

```bash
cd /home/groby/dev/isched/cmake-build-debug
ctest --output-on-failure
```

## Manual Verification Checklist (optional)

- On an authenticated route, top nav shows logo from assets and primary menu entries.
- Only one sign-out button is present on authenticated screens because the shared shell owns global nav chrome.
- Active menu state updates when navigating between `Dashboard`, `Organizations`, `Users`, and `RBAC`.
- Bottom status bar always shows non-empty identity text.
- Trigger organization-user fetch and observe digest transition from loading to success wording.
- Simulate failure path and verify digest becomes failure-oriented and understandable.
- Resize viewport and verify digest/user regions remain readable and non-overlapping.

## Risks to Watch During Validation

- Missing digest publications from some pages can leave stale status text.
- Identity fallback may persist incorrectly if session refresh path does not emit resolved user name.
- Existing page-local nav elements may conflict visually with new global shell until cleanup is complete.

## Recommended final regression sequence

```bash
cd /home/groby/dev/isched/src/ui
pnpm run test:shell
pnpm run test
pnpm run e2e:shell
pnpm run e2e:dev-proxy
pnpm run e2e

cd /home/groby/dev/isched/cmake-build-debug
ctest --output-on-failure
```
