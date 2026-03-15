# Implementation Plan: Seed Mode and Administration UI

**Branch**: `003-seed-mode-admin-ui` | **Date**: 2026-03-15 | **Spec**: [Seed Mode and Administration UI](spec.md)
**Tasks**: [tasks.md](tasks.md) — 40 tasks across 6 phases, all open
**Status**: Ready to implement — spec, research, and tasks complete; prerequisite checklist must be verified before Phase A begins

---

## Summary

Add a self-contained Angular 21 administration UI served at `/isched`, embedded in the `isched_srv` binary at compile time. The UI provides a seed form (first-run, no admin exists), a login form, and a minimal RBAC dashboard. Backend changes: add `systemState { seedModeActive }` GraphQL field, harden `createPlatformAdmin` with seed-mode gate and rate limiting, extend startup log, and implement static asset serving with ETag support.

---

## Technical Context

Carries forward all constraints from story 001:

- **Language**: C++23, C++ Core Guidelines
- **Build**: CMake + Ninja + Conan 2.x
- **HTTP transport**: Boost.Beast async server (sole server transport; `cpp-httplib` is test-client only — Q13)
- **GraphQL**: custom PEGTL parser + `GqlExecutor`
- **Storage**: `isched_system.db` (`platform_admins`, `platform_roles`, `organizations`) + per-tenant SQLite
- **Auth**: JWT via `jwt-cpp`, Argon2id via OpenSSL 3.5
- **Angular**: 21.0.1 standalone component API, Angular Router, HTML5 history mode; `src/ui/`
- **Styles**: DaisyUI 4.x / Tailwind CSS 3.x (theme: `corporate`)
- **Test runner (Angular)**: Jest via `@angular-builders/jest`; headless; `pnpm --filter isched-ui run test`
- **Embedding**: `tools/embed_ui_assets.py` generates `isched_ui_assets.hpp` via SHA-256 ETag + `xxd -i` (Python fallback)
- **Rate limiter**: `TokenBucket` keyed on `ResolverCtx::remote_ip`; configurable via `ISCHED_SEED_RATE_LIMIT` → `server.seed_rate_limit` → default 5 (Q17)
- **pnpm workspace**: `pnpm-workspace.yaml` extended to include `src/ui` (Q18)

---

## Phase Overview

| Phase | Name | Tasks | Purpose |
|---|---|---|---|
| A | Angular subproject bootstrap | 6 (T-UI-A-001–006) | Create `src/ui/` Angular 21 app, pnpm workspace wiring, Tailwind + DaisyUI, Jest |
| B | CMake build integration | 5 (T-UI-B-001–005) | `isched_ui_build` + `isched_ui_embed` targets, `embed_ui_assets.py`, asset registry header |
| C | C++ static asset serving | 3 (T-UI-C-001–003) | `UiAssetRegistry` class, `/isched` Boost.Beast GET handler, ETag/304, push-state fallback, startup log |
| D | Backend GraphQL additions | 5 (T-UI-D-001–005) | `remote_ip` in `ResolverCtx`, `systemState` field, `createPlatformAdmin` resolver, `TokenBucket` rate limiter |
| E | Angular application | 9 (T-UI-E-001–015) | Three screens (seed, login, dashboard), route guard, services, interceptors |
| F | Testing | 10 (T-UI-F-001–014) | C++ integration tests (`test_admin_ui.cpp`, `test_seed_mode.cpp`), Angular Jest component tests |

**Total**: 38 implementation tasks + 2 testing registration tasks = 40 tasks

---

## Prerequisite Checklist

*(Must be confirmed before Phase A begins)*

- [ ] `node --version` ≥ 22 on all developer machines
- [ ] `pnpm --version` ≥ 10
- [ ] `xxd` available on build host (or Python fallback confirmed in Phase B)
- [ ] Angular CLI 21 compatible with the installed Node version
- [ ] Existing `ctest` suite 29/29 green on `master`

---

## Constitution Check

*GATE: Must pass before implementation starts. Re-check after any design updates.*

✅ **GraphQL-only transport**: All data operations remain through `/graphql`; `/isched` is read-only static asset serving only — no data API at that path  
✅ **Security-first**: `createPlatformAdmin` is seed-mode gated (FORBIDDEN when inactive), rate-limited (TokenBucket), Argon2id hashed; seed form only visible when `seedModeActive: true`  
✅ **C++ Core Guidelines**: `UiAssetRegistry` uses `std::span`, `std::string_view`; `RateLimiter` protected by `std::mutex`; no raw `new`/`delete`  
✅ **Test-driven**: Every resolver and every Angular component has a corresponding test task in Phase F  
✅ **Single-process**: UI assets are compiled into the binary — no separate web server process  
✅ **No outdated components**: Angular 21.0.1, DaisyUI 4.x, Tailwind CSS 3.x — all current at time of spec  
✅ **OWASP alignment**: CSP meta tag (SR-UI-004), `X-Content-Type-Options`, `X-Frame-Options`, ETag without `no-store` (SR-UI-005), no credentials in URLs
