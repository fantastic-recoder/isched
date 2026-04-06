# isched WebUI 004 Core Validation (2026-04-05)

Focused validation pass for outstanding core WebUI deliverables on feature `004-add-isched-webui`, with priority on merged `003` behavior continuity and T022 completion.

## Completed in this pass

- Added dedicated integration suite: `src/test/cpp/integration/test_webui_embedded_serving.cpp`
- Extended legacy embedded-serving coverage in `src/test/cpp/integration/test_admin_ui.cpp` with canonical routing assertions (`GET /`, `GET /graphql` redirect to `/isched`; `POST /graphql` remains API)
- Registered suite in build/test graph: `src/test/cpp/isched/CMakeLists.txt`
- Marked task complete: `specs/archive/004-add-isched-webui/tasks.md` (`T022` -> `[X]`)

## Validation summary

- Angular unit tests (`pnpm test`): PASS
- Playwright bootstrap suite (`pnpm e2e:bootstrap`): PASS
- Relevant C++ integration tests:
  - `test_webui_embedded_serving`: PASS
  - `test_admin_ui`: PASS
  - `test_seed_mode`: PASS
  - `test_bootstrap_platform_admin`: PASS

## Additional gap-closure pass (T068/T069/T070/T071/T072/T073)

- Implemented embedded-serving contract hardening in backend runtime (`/isched`): startup diagnostics line, explicit missing-asset `404` JSON envelope vs SPA fallback split, CSP + hardening headers, and robust `ETag`/`If-None-Match` handling.
- Implemented auth lockout/rate-limit configuration precedence chain and diagnostics logging.
- Added integration coverage for:
  - embedded route-contract distinctions and security/cache headers
  - lockout precedence chain behavior
- Focused verification run:
  - `test_auth_lockout_policy`: PASS
  - `test_webui_embedded_serving`: PASS
  - `test_admin_ui`: PASS

## Detailed evidence

- `specs/archive/004-add-isched-webui/evidence/t022-embedded-serving-validation-2026-04-05.md`
- `specs/archive/004-add-isched-webui/evidence/t073-gap-closure-validation-2026-04-05.md`

