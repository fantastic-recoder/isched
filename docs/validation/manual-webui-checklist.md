# Manual WebUI testing checklist

Run against the embedded UI, not only the Angular development server: the embedded
server applies the production Content Security Policy (CSP).

Record the commit, browser/version, date, data directory, and any failed item IDs.
Check an item only after observing its expected result. For failures, record the
actual result, browser console error, and relevant GraphQL response; redact
passwords, cookies, and tokens.

## 1. Build and start an isolated server

Run commands from the repository root. Use only disposable local test data and
credentials. Never reset a production or shared database.

```bash
python3 configure.py
cmake --build cmake-build-debug --target isched_ui_embed hello_plugin
TEST_DATA_DIR="$(mktemp -d -t isched-manual-XXXXXX)"
printf 'Test data directory: %s\n' "$TEST_DATA_DIR"
ISCHED_SERVER_PORT=18080 ./cmake-build-debug/src/main/cpp/isched/isched_srv \
  --data-dir "$TEST_DATA_DIR" \
  --seed 'manual-admin@example.test:ManualTest-Only-2026'
```

The credentials above are public examples for local testing only. CLI credentials
are visible in shell history/process arguments; do not substitute real credentials.
At `really wipe out platform DB and reset admin password?`, type exactly `yes`.
Leave the server running in this terminal.

- [ ] START-01: Server starts without errors and reports port `18080`.
- [ ] START-02: Open `http://127.0.0.1:18080/isched/`; the seeded instance shows Sign In, not Initialize Platform.
- [ ] START-03: Open browser DevTools Console and Network, enable Preserve log, and confirm UI assets load successfully.
- [ ] START-04: In a separate disposable directory, invoke the same seed command but answer `no`; reset is aborted and the server does not start.

### Alternative: test first-admin bootstrap through the UI

Stop the first server with Ctrl+C before using the same port. Do not reuse the
seeded directory for this scenario.

```bash
BOOTSTRAP_DATA_DIR="$(mktemp -d -t isched-bootstrap-manual-XXXXXX)"
ISCHED_SERVER_PORT=18080 ./cmake-build-debug/src/main/cpp/isched/isched_srv \
  --data-dir "$BOOTSTRAP_DATA_DIR"
```

- [ ] BOOT-01: Open `/isched/`; the uninitialized instance routes to Initialize Platform.
- [ ] BOOT-02: Submit empty fields, an invalid email, and a password shorter than 12 characters; validation appears and no admin is created.
- [ ] BOOT-03: Enter a valid test email, display name, and password of at least 12 characters; Complete Bootstrap succeeds and allows authenticated access.
- [ ] BOOT-04: Sign out and revisit `/isched/bootstrap`; a second platform admin cannot be bootstrapped.

Continue the remaining sections with either initialized instance.

## 2. Authentication and navigation

- [ ] AUTH-01: Sign in with an incorrect password once; an error appears and no authenticated dashboard is shown.
- [ ] AUTH-02: Sign in with the initialized admin credentials; Dashboard, health, and version load without GraphQL errors.
- [ ] AUTH-03: Navigate through Organizations, Users, RBAC, and GraphQL Playground; each page renders without an uncaught console error.
- [ ] AUTH-04: Reload a deep link such as `/isched/playground`; the SPA loads and either restores the session or clearly requests sign-in.
- [ ] AUTH-05: Sign out, then directly open `/isched/dashboard` and `/isched/playground`; protected content is inaccessible.
- [ ] AUTH-06: Inspect browser storage; access JWTs are not persisted in localStorage, sessionStorage, or IndexedDB.

## 3. Organizations

Use the dedicated Organizations administration page first. Test the dashboard
dialog separately so that a working page does not mask a broken dialog.

- [ ] ORG-01: Empty and one-character organization names cannot be submitted.
- [ ] ORG-02: Create `Manual Test Alpha`; it appears once with ACTIVE status and survives a page reload.
- [ ] ORG-03: Create `Manual Test Beta`; search finds each organization and clearing search restores the list.
- [ ] ORG-04: Edit Alpha to `Manual Test Alpha Edited`; Save Organization updates the list and revision.
- [ ] ORG-05: Begin another edit and press Cancel; unsaved changes are not stored.
- [ ] ORG-06: Deactivate Alpha, confirm its status, then reactivate it; each change persists after reload.
- [ ] ORG-07: Expand Dashboard > Organizations and click + Create Organization; the dialog opens. Cancel and Escape close it without a mutation or CSP error.
- [ ] ORG-08: Create a separate organization through that dashboard dialog; it appears in both the dashboard and dedicated administration page.

## 4. Users and roles

- [ ] USER-01: On Users, select Alpha; create a user with Login ID `manual-user` and Display Name `Manual User`. The user appears in the selected organization.
- [ ] USER-02: Empty display name or a Login ID shorter than two characters cannot be submitted.
- [ ] USER-03: Try a duplicate Login ID within Alpha; a visible error appears and no duplicate row is created.
- [ ] USER-04: Edit the display name, save, search for the user, and reload; the saved value remains.
- [ ] USER-05: Deactivate and reactivate the user; status and action availability update correctly.
- [ ] USER-06: Switch to Beta; Alpha's user is not listed as a Beta member.
- [ ] USER-07: Expand an organization on Dashboard and open its Create User dialog; Cancel and Escape close it without submitting. Invalid email/short password show validation.
- [ ] RBAC-01: Open RBAC; built-in roles and their scopes load.
- [ ] RBAC-02: Create a custom test role with a unique ID and supported scope; it appears in the role list and can be edited.
- [ ] RBAC-03: Assign the role to the intended test user's ID; the UI reports the actual outcome and the assignment can be verified after reload.

## 5. Plugins and dialog cancellation

Get the absolute library path with
`realpath cmake-build-debug/src/example/cpp/libhello_plugin.so`.
This must be a path on the server machine, not a browser upload.

- [ ] PLUGIN-01: Expand Dashboard > Plugins; + Load Plugin opens the dialog.
- [ ] PLUGIN-02: Immediately press Cancel with an empty form; the dialog closes, focus returns to a usable dashboard control, and no `loadPlugin` mutation is sent.
- [ ] PLUGIN-03: Reopen, fill the fields, then Cancel; no plugin is loaded. Reopening resets the form.
- [ ] PLUGIN-04: Reopen and press Escape; the dialog closes without a mutation. Repeat using the backdrop.
- [ ] PLUGIN-05: Submit an empty library path; no load occurs. Submit a nonexistent path; a visible error appears, pending state clears, and Cancel still works.
- [ ] PLUGIN-06: Load the built `libhello_plugin.so` using kind RESOLVER and Plugin ID `manual-hello`; the dialog closes and one Active plugin row appears.
- [ ] PLUGIN-07: Expand the plugin details; ID, kind, library path, and metadata agree with the loaded library.
- [ ] PLUGIN-08: Select the plugin and Unload Selected; it becomes inactive and the button state updates.
- [ ] PLUGIN-09: Throughout cancellation and loading, Console has no CSP inline-handler violation or uncaught exception.

## 6. GraphQL browser: live editing and execution

Open `/isched/playground`. Editing happens locally; Run explicitly submits the
current editor text to `/graphql`.

```graphql
query ManualHealth {
  isched {
    health { status }
    version
  }
}
```

- [ ] GQL-01: Schema/type tree, query editor, and JSON result panel all render; introspection completes without an error.
- [ ] GQL-02: Paste the query above and click Run; JSON contains `data.isched.health.status` and `data.isched.version` with no GraphQL errors.
- [ ] GQL-03: Edit the query online by removing `version`, then Run; the new response omits `version` and Network shows the edited text, not the previous query.
- [ ] GQL-04: Add alias `serverVersion: version` and Run; the response contains `serverVersion`, demonstrating that live edits are used.
- [ ] GQL-05: Select a schema field and click Generate Query; a corresponding stub appears. Complete any required arguments, edit the selection, and execute it successfully.
- [ ] GQL-06: Check typing, selection, copy/paste, undo/redo, indentation, and syntax highlighting; editor content remains usable.
- [ ] GQL-07: Submit malformed syntax, then an unknown field; each produces a visible error rather than a blank or success-looking result. Correct the text and successfully rerun.
- [ ] GQL-08: Run an empty editor and rapidly click Run on a nonempty query; no empty request or duplicate in-flight execution is sent.
- [ ] GQL-09: Resize both panel dividers, navigate away and back, and reload; saved panel proportions are restored and no panel becomes unusable.
- [ ] GQL-10: Keep Playground open in a second authenticated tab, then load/unload the example plugin on Dashboard; its schema contributions appear/disappear without manually reloading Playground.
- [ ] GQL-11: Enter a subscription operation and Run; the UI clearly explains whether this editor supports subscriptions instead of silently showing no result.
- [ ] GQL-12: With a query entered, temporarily block `/graphql` in DevTools or stop the server; Run shows a transport error, clears the spinner, and preserves editor text.
- [ ] GQL-13: Restore connectivity (restart with the same data directory **without `--seed`** if needed); retry or rerun succeeds without losing the draft.

## 7. Usability, persistence, and cleanup

- [ ] UX-01: At desktop and narrow/mobile widths, forms, navigation, dialogs, and Playground controls remain reachable and readable.
- [ ] UX-02: Use only Tab, Shift+Tab, Enter, and Escape for sign-in and plugin cancellation; focus is visible and modal focus does not escape into the page.
- [ ] UX-03: Toggle dashboard panels, navigate away/back, and reload; expansion state is restored.
- [ ] UX-04: Stop and restart the server with the same `--data-dir` but no `--seed`; organizations, users, and role changes remain. Sign-in still works.
- [ ] UX-05: Review Console and Network for unexpected errors; external application API traffic uses `/graphql`, not REST endpoints.
- [ ] CLEAN-01: Sign out, unload test plugins if still loaded, and stop the test server.
- [ ] CLEAN-02: Record failed item IDs and reproduction details. Remove only the exact disposable data directories created for this run after retaining any needed evidence.

Unchecked items are not passes. Missing plugin builds, unavailable backend features,
or browser limitations should be recorded as blocked with a reason, not skipped silently.
