# Archived Feature Specifications

Completed or historically retired feature-spec artifacts are moved here once they are no longer active under `specs/`.

## Archived Features

- `001-universal-backend` — core GraphQL-only backend architecture, transport, auth, configuration, and closeout evidence
- `002-comparable-benchmark` — comparable benchmark tooling and benchmark acceptance workflow for isched vs Apollo Server
- `003-seed-mode-admin-ui` — historical seed-mode/admin-UI specification retained as an archived planning record
- `004-add-isched-webui` — embedded Angular WebUI feature artifacts and evidence retained as an archived implementation record
- `005-rate-limited-auth-bootstrap` — auth/bootstrap consistency and lockout-guidance feature artifacts retained as an archived implementation record
- `006-upload-schema` — tenant-admin schema document upload, list, and fetch support with tenant-scoped isolation and validation evidence
- `007-webui-nav-status-bars` — shared authenticated top navigation and bottom status bar, signal-first shell status publication, and focused unit/Playwright regression coverage
- `008-dod-mech-refactor` — data-oriented design and mechanical sympathy refactoring passes for the C++ backend; hot-path analysis, SoA transition, pointer-to-index replacement, contiguous memory locality, and ≥80% line/branch coverage target
- `009-raise-pass-improvement-ratio` — SC-002 recovery and improvement-ratio compliance closure follow-up after `008-dod-mech-refactor`; stateless system function correctness and final coverage gates
- `010-ast-enum-dispatch` — typed PEGTL AST node dispatch refactor replacing dynamic_cast polymorphism with enum-discriminated union dispatch for zero-cost GraphQL query traversal
- `011-graphql-playground` — interactive 3-panel GraphQL Playground at `/playground`: schema/type tree (introspection), CodeMirror 6 query editor, JSON result panel; resizable splits with localStorage persistence; subscription advisory detection
- `013-lsp-graphql-angular` — completed schema-aware GraphQL editing, organization/user administration, cookie-only HTTP/WS sessions, exact-Origin and session-bound CSRF controls; all 70 tasks complete with automated acceptance, performance, and constitution-review evidence. Manual acceptance testing follows separately.
