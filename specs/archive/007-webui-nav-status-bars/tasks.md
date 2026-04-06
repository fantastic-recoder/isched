# Tasks: WebUI Navigation + Status Bars

**Input**: Design documents from `/home/groby/dev/isched/specs/007-webui-nav-status-bars/`  
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`, `contracts/webui-shell-contract.md`, `quickstart.md`

**Tests**: Explicit unit/component and Playwright smoke coverage is required for this feature. Each user story includes the verification tasks needed before that story can be considered complete.

**Organization**: Tasks are grouped by setup, foundational work, and user story so each story remains independently implementable and testable.

## Format: `[ID] [P?] [Story] Description`

- `[P]` indicates a task that can run in parallel with other tasks that touch different files and have no unfinished prerequisites.
- `[Story]` labels appear only in user story phases (for example, ``[US1]``).
- Every task includes exact file paths so `speckit.implement` can execute them directly.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Prepare focused validation surfaces and execution commands for the shell feature before code changes begin.

- [x] T001 Refresh the shell validation matrix and focused run commands in `specs/007-webui-nav-status-bars/quickstart.md`
- [x] T002 [P] Add focused shell unit/smoke scripts for direct execution in `src/ui/package.json`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Create the shared shell state, identity plumbing, and root composition required by all stories.

**CRITICAL**: Complete this phase before starting user story implementation.

- [x] T003 Create shell domain types and primary navigation definitions in `src/ui/src/app/services/shell-status.models.ts`
- [x] T004 [P] Implement a signal-first `ShellStatusService` with latest-wins digest sequencing and fallback identity state in `src/ui/src/app/services/shell-status.service.ts`
- [x] T005 [P] Extend authenticated session bootstrap/sign-out flows to expose current-user `displayName` and drive shell identity state in `src/ui/src/app/services/auth.service.ts`
- [x] T006 [P] Create the standalone authenticated shell component with separate template/style files in `src/ui/src/app/components/authenticated-shell/authenticated-shell.component.ts`, `src/ui/src/app/components/authenticated-shell/authenticated-shell.component.html`, and `src/ui/src/app/components/authenticated-shell/authenticated-shell.component.scss`
- [x] T007 Wire authenticated-route detection and shell outlet composition in `src/ui/src/app/app.ts`, `src/ui/src/app/app.html`, and `src/ui/src/app/app.scss`

**Checkpoint**: Shared shell primitives are ready; user stories can now proceed in priority order or in parallel with coordination on shared shell files.

---

## Phase 3: User Story 1 - Persistent App Shell Navigation (Priority: P1) 🎯 MVP

**Goal**: Deliver a shared authenticated shell with a persistent top navigation bar, isched logo, active-route highlighting, and no duplicated page-local navigation chrome.

**Independent Test**: Sign in or bootstrap into an authenticated route, verify the top bar appears on `/dashboard` and `/admin/*`, confirm the asset logo is visible, use the menu to navigate between destinations, and confirm the active destination is visually clear without any duplicate local navbar.

### Tests for User Story 1

- [x] T008 [P] `[US1]` Add root-app shell visibility tests for authenticated versus unauthenticated routes in `src/ui/src/app/app.spec.ts`
- [x] T009 [P] `[US1]` Add authenticated-shell navigation render, active-route, and sign-out tests in `src/ui/src/app/components/authenticated-shell/authenticated-shell.component.spec.ts`

### Implementation for User Story 1

- [x] T010 [P] `[US1]` Implement the DaisyUI top navbar with asset logo, primary menu links, and active-route classes in `src/ui/src/app/components/authenticated-shell/authenticated-shell.component.html` and `src/ui/src/app/components/authenticated-shell/authenticated-shell.component.scss`
- [x] T011 [P] `[US1]` Implement shell navigation state and sign-out behavior with signal-backed view state in `src/ui/src/app/components/authenticated-shell/authenticated-shell.component.ts`
- [x] T012 `[US1]` Refactor root app composition so authenticated routes use the shared shell as the single navigation source in `src/ui/src/app/app.ts`, `src/ui/src/app/app.html`, and `src/ui/src/app/components/authenticated-shell/authenticated-shell.component.ts`
- [x] T013 `[US1]` Remove dashboard-local navbar/sign-out chrome and keep dashboard content layout compatible with the shared shell in `src/ui/src/app/pages/dashboard/dashboard.ts`, `src/ui/src/app/pages/dashboard/dashboard.html`, and `src/ui/src/app/pages/dashboard/dashboard.scss`

**Checkpoint**: Authenticated users can navigate core areas from the global top shell without duplicate page-local navigation UI.

---

## Phase 4: User Story 2 - Status Visibility and Context (Priority: P1)

**Goal**: Deliver a persistent bottom status bar that always shows a non-empty current-user label and the latest tracked operation digest with deterministic wording.

**Independent Test**: Load an authenticated screen, confirm the bottom bar shows a non-empty user label, trigger organization-user loading, observe `Loading organization users` then `Organization users loaded`, and verify failure/rapid-update cases keep the newest understandable digest visible.

### Tests for User Story 2

- [x] T014 [P] `[US2]` Add `ShellStatusService` unit tests for loading/success/error transitions, latest-wins sequencing, and identity fallback in `src/ui/src/app/services/shell-status.service.spec.ts`
- [x] T015 [P] `[US2]` Extend shell and users-page tests for bottom status digest text and current-user label behavior in `src/ui/src/app/app.spec.ts`, `src/ui/src/app/components/authenticated-shell/authenticated-shell.component.spec.ts`, and `src/ui/src/app/pages/admin/users.page.spec.ts`

### Implementation for User Story 2

- [x] T016 [P] `[US2]` Implement the DaisyUI/Tailwind bottom status bar layout, truncation rules, and polite live-region semantics in `src/ui/src/app/components/authenticated-shell/authenticated-shell.component.html` and `src/ui/src/app/components/authenticated-shell/authenticated-shell.component.scss`
- [x] T017 [P] `[US2]` Populate shell identity from authenticated session bootstrap and clear it back to fallback state on sign-out in `src/ui/src/app/services/auth.service.ts` and `src/ui/src/app/services/shell-status.service.ts`
- [x] T018 [P] `[US2]` Emit normalized organization-user loading/success/error digests from GraphQL user-list calls in `src/ui/src/app/services/user.service.ts`
- [x] T019 `[US2]` Coordinate `UsersPage` organization-switch and initial-load flows with shell digest publication without duplicating status UI in `src/ui/src/app/pages/admin/users.page.ts` and `src/ui/src/app/pages/admin/users.page.html`

**Checkpoint**: Authenticated screens now show a persistent bottom status bar with deterministic digest and identity behavior.

---

## Phase 5: User Story 3 - Confidence Through Automated UI Validation (Priority: P2)

**Goal**: Add focused automated coverage that prevents regressions in the global shell, digest behavior, and current-user visibility.

**Independent Test**: Run the focused shell unit suites and the shell Playwright smoke suite, and verify they cover top navigation, bottom status bar presence, current-user display, and the representative organization-user digest transition.

### Tests for User Story 3

- [x] T020 [P] `[US3]` Add auth/session integration tests for resolved display-name replacement and sign-out fallback reset in `src/ui/src/app/services/auth.service.spec.ts` and `src/ui/src/app/services/shell-status.service.spec.ts`
- [x] T021 [P] `[US3]` Add user-service digest publication tests for organization-user loading, success, and failure sequences in `src/ui/src/app/services/user.service.spec.ts`
- [x] T022 [P] `[US3]` Add Playwright smoke coverage for shell navigation, status bar, current-user label, and organization-user digest transitions in `src/ui/e2e/shell-smoke.spec.ts`

### Implementation for User Story 3

- [x] T023 `[US3]` Add stable shell/status automation selectors and ARIA hooks for smoke coverage in `src/ui/src/app/components/authenticated-shell/authenticated-shell.component.html` and `src/ui/src/app/pages/admin/users.page.html`
- [x] T024 `[US3]` Add focused shell unit/smoke execution scripts in `src/ui/package.json`
- [x] T025 `[US3]` Reuse authenticated Playwright bootstrap helpers for the shell smoke flow in `src/ui/e2e/bootstrap.spec.ts` and `src/ui/e2e/global-setup.ts`

**Checkpoint**: Automated unit and smoke coverage protects the new global shell and its representative digest behavior.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Finalize developer guidance and the end-to-end validation checklist for the feature.

- [x] T026 [P] Update shared-shell ownership, signal-first status publication guidance, and shell smoke commands in `src/ui/README.md`
- [x] T027 Update final regression commands and validation notes for `pnpm run test`, focused shell tests, `pnpm run e2e -- e2e/shell-smoke.spec.ts`, and `ctest --output-on-failure` in `specs/007-webui-nav-status-bars/quickstart.md`
- [x] T028 Add explicit dev-server proxy preservation coverage for `/graphql` routing in `src/ui/e2e/dev-proxy.spec.ts`, `src/ui/proxy.conf.json`, and `src/ui/package.json`
- [x] T029 Review the shared-shell refactor for clean code compliance in `src/ui/src/app/`, verifying small/focused functions, preference for focused service/component boundaries over complex conditionals, and documenting that no profiling-backed hot-path abstraction exception was needed

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies.
- **Phase 2 (Foundational)**: Depends on Phase 1 and blocks all user stories.
- **Phase 3 (US1)**: Depends on Phase 2.
- **Phase 4 (US2)**: Depends on Phase 2.
- **Phase 5 (US3)**: Depends on Phase 2 and is most valuable after US1 and US2 behavior is in place.
- **Phase 6 (Polish)**: Depends on the completion of the desired user stories.

### User Story Dependency Graph

- **US1 (P1)**: Depends only on foundational shell state/composition from Phase 2.
- **US2 (P1)**: Depends only on foundational shell state/composition from Phase 2.
- **US3 (P2)**: Depends on the shell surfaces from US1 and the digest/identity behavior from US2.

### Suggested Completion Order

1. Complete Phase 1 and Phase 2.
2. Deliver **US1** as the MVP shell navigation increment.
3. Deliver **US2** to add persistent status context.
4. Deliver **US3** to lock behavior down with automated validation.
5. Finish Phase 6 polish and full regression guidance.

### Within Each User Story

- Create or extend the story-specific tests before marking the story complete.
- Finish shared service/state wiring before page-level shell integration.
- Remove duplicated page-local nav/status chrome before final story sign-off.
- Keep template state signal-backed and use separate `.html` / `.scss` files for new or refactored components.

---

## Parallel Opportunities

- **Setup**: T001 and T002 can proceed in parallel.
- **Foundational**: T004, T005, and T006 can proceed in parallel after T003; T007 follows once the shared shell artifacts exist.
- **US1**: T008 and T009 can proceed in parallel; T010 and T011 can proceed in parallel after T006.
- **US2**: T014 and T015 can proceed in parallel; T016, T017, and T018 can proceed in parallel once foundational shell files exist.
- **US3**: T020, T021, and T022 can proceed in parallel; T024 can proceed in parallel with T025 once selectors are stable.
- **Polish**: T026 can proceed in parallel with the final validation update in T027.

---

## Parallel Example: User Story 1

```bash
# Parallel US1 test authoring
Task T008: src/ui/src/app/app.spec.ts
Task T009: src/ui/src/app/components/authenticated-shell/authenticated-shell.component.spec.ts

# Parallel US1 implementation split after the shell scaffold exists
Task T010: src/ui/src/app/components/authenticated-shell/authenticated-shell.component.html + src/ui/src/app/components/authenticated-shell/authenticated-shell.component.scss
Task T011: src/ui/src/app/components/authenticated-shell/authenticated-shell.component.ts
```

## Parallel Example: User Story 2

```bash
# Parallel US2 verification work
Task T014: src/ui/src/app/services/shell-status.service.spec.ts
Task T015: src/ui/src/app/app.spec.ts + src/ui/src/app/components/authenticated-shell/authenticated-shell.component.spec.ts + src/ui/src/app/pages/admin/users.page.spec.ts

# Parallel US2 implementation split
Task T016: src/ui/src/app/components/authenticated-shell/authenticated-shell.component.html + src/ui/src/app/components/authenticated-shell/authenticated-shell.component.scss
Task T017: src/ui/src/app/services/auth.service.ts + src/ui/src/app/services/shell-status.service.ts
Task T018: src/ui/src/app/services/user.service.ts
```

## Parallel Example: User Story 3

```bash
# Parallel US3 coverage work
Task T020: src/ui/src/app/services/auth.service.spec.ts + src/ui/src/app/services/shell-status.service.spec.ts
Task T021: src/ui/src/app/services/user.service.spec.ts
Task T022: src/ui/e2e/shell-smoke.spec.ts

# Parallel US3 execution/harness follow-up
Task T024: src/ui/package.json
Task T025: src/ui/e2e/bootstrap.spec.ts + src/ui/e2e/global-setup.ts
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1 and Phase 2.
2. Complete US1 tasks T008-T013.
3. Validate the authenticated shell renders on `/dashboard` and `/admin/*` with logo, active navigation, and no duplicate page navbar.
4. Stop and demo the shared top navigation shell before adding bottom status behavior.

### Incremental Delivery

1. Build the shared shell foundation (Phases 1-2).
2. Deliver US1 for persistent top navigation.
3. Deliver US2 for persistent bottom status visibility and digest behavior.
4. Deliver US3 for unit and smoke regression confidence.
5. Finish polish/docs and run the full validation sequence.

### Implementation Readiness Checklist

- All tasks follow the required checklist format: `- [ ] T### [P?] [Story] Description with file path`.
- Every user story has explicit verification tasks and implementation tasks.
- The dependency order supports MVP-first delivery and multi-developer parallelization.
- The task list explicitly covers signal-first Angular state, separate template/style files, DaisyUI/Tailwind styling, smoke Playwright coverage, and removal of duplicated route/page shell chrome.
