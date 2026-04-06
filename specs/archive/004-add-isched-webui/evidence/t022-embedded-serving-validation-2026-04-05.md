# Validation Evidence: T022 Embedded Serving

**Feature**: `004-add-isched-webui`  
**Task**: `T022`  
**Date**: 2026-04-05

## Scope

Validated the dedicated embedded-serving integration coverage requested by T022:
- Static embedded asset serving under `/isched`
- SPA fallback routing for client routes
- Bootstrap route availability semantics tied to backend seed/bootstrap state
- Canonical browser-entry redirects (`GET /`, `GET /graphql` -> `/isched`) while preserving `POST /graphql` API behavior

## Implemented Artifact

- `src/test/cpp/integration/test_webui_embedded_serving.cpp`

## Command Log (Focused)

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

## Results

- `ctest -R "^test_webui_embedded_serving$"`: **PASS**
- `ctest -R "^test_admin_ui$"`: **PASS**
- `ctest -R "^test_seed_mode$"`: **PASS**
- `ctest -R "^test_bootstrap_platform_admin$"`: **PASS**
- `pnpm test`: **PASS** (11 suites / 46 tests)
- `pnpm e2e:bootstrap`: **PASS** (2 tests)

## Notes

- Playwright bootstrap suite initially failed before the backend binary existed at `cmake-build-debug/src/main/cpp/isched/isched_srv`; after building target `isched_srv`, the suite passed.
- Embedded-serving test fixture now uses an isolated temporary `work_directory` per test case to keep seed-mode/bootstrap assertions deterministic.
- Canonical redirect checks are covered by both `test_webui_embedded_serving` and `test_admin_ui` to keep legacy `003` and current `004` expectations aligned.

