# IschedUi

This project was generated using [Angular CLI](https://github.com/angular/angular-cli) version 21.1.3.

## Tech Stack

- **Angular 21** with standalone components, signals, and strict TypeScript
- **Tailwind CSS 3.x** + **DaisyUI 4.x** for styling — use DaisyUI component classes (`btn`, `card`, `alert`, `modal`, `table`, etc.) and extend with Tailwind utilities
- **DaisyUI theme**: `corporate` (set via `data-theme="corporate"` on `<html>`)
- **GraphQL-only** backend integration via `/graphql` endpoint
- **Embedded runtime route**: backend serves the built SPA at `/isched` (for example `http://localhost:8080/isched`)

## Coding Conventions

- **Templates and styles MUST be in separate files** — use `templateUrl` and `styleUrl`, never inline `template` or `styles`. Each component gets a `.html` and `.scss` file alongside its `.ts` file.
- Signal-first state management; RxJS only when stream semantics are needed.
- Standalone components/directives/pipes; no NgModule-centric architecture.
- Modern template control flow: `@if`, `@for`, `@switch`.
- Typed reactive forms for user input flows.
- `OnPush` change detection where feasible.

## Shared authenticated shell ownership

- Authenticated route chrome lives in `src/app/components/authenticated-shell/` and is the **single source** for the top navigation bar, sign-out entry point, and bottom status bar.
- Do not add page-local navbars or page-local sign-out controls to authenticated pages such as `dashboard` or `admin/*`.
- Route-level pages should focus on page content and publish tracked operation state through services, not through duplicated shell UI.

## Signal-first shell status publication

- `ShellStatusService` owns the app-shell digest and current-user identity signals.
- Auth/session flows update shell identity through `AuthService.bootstrapSession()` and reset it on sign-out/session loss.
- Tracked long-running operations should publish deterministic shell digests through shared services (for example `UserService.listUsers()` publishes `Loading organization users` → `Organization users loaded`).
- When an operation can overlap with a newer request, preserve latest-wins behavior by carrying the sequence returned from `ShellStatusService.beginOperation()` into the corresponding success/error publication.

## Development server

To start a local development server, run:

```bash
pnpm start
```

Once the server is running, open your browser and navigate to `http://localhost:4200/`. The application will automatically reload whenever you modify any of the source files.

### GraphQL Proxy Rule (Required)

- Frontend GraphQL calls must use the relative path `/graphql` only.
- Do not hard-code backend origins in Angular services.
- `pnpm start` runs `ng serve --proxy-config proxy.conf.json` so HTTP and WebSocket traffic on `/graphql` is proxied to local backend `:8080`.

```bash
cd /home/groby/dev/isched/src/ui
pnpm start
```

## Code scaffolding

Angular CLI includes powerful code scaffolding tools. To generate a new component, run:

```bash
ng generate component component-name
```

For a complete list of available schematics (such as `components`, `directives`, or `pipes`), run:

```bash
ng generate --help
```

## Building

To build the project run:

```bash
ng build
```

This will compile your project and store the build artifacts in the `dist/` directory. By default, the production build optimizes your application for performance and speed.

## Running unit tests

To execute unit tests with the [Jest](https://jestjs.io/) test runner (via `@angular-builders/jest`), use the following command:

```bash
ng test
```

Run the focused shared-shell suites directly:

```bash
cd /home/groby/dev/isched/src/ui
pnpm run test:shell
```

## Running Playwright integration tests

Playwright tests start the real backend server (`isched_srv`) with a temporary `--data-dir` so the system starts in seed mode and the bootstrap UI can be validated end-to-end.

```bash
cd /home/groby/dev/isched/src/ui
pnpm e2e:install
pnpm e2e:bootstrap
```

If `pnpm e2e:install` fails because distro package managers are unavailable in your environment, install only the browser binaries:

```bash
cd /home/groby/dev/isched/src/ui
pnpm exec playwright install chromium
```

Run the complete Playwright suite:

```bash
cd /home/groby/dev/isched/src/ui
pnpm e2e
```

Run only the shared-shell smoke coverage:

```bash
cd /home/groby/dev/isched/src/ui
pnpm run e2e:shell
```

### Selecting a different CMake build directory

By default the test harness resolves the server binary from the debug build tree:

```
cmake-build-debug/src/main/cpp/isched/isched_srv
```

To use a different build directory (e.g. a release or CI build), set `ISCHED_BUILD_DIR` before running the tests:

```bash
ISCHED_BUILD_DIR=cmake-build-release pnpm e2e
# or
ISCHED_BUILD_DIR=/path/to/custom-build pnpm e2e
```

The path is resolved relative to the repository root.

### Environment variables summary

| Variable | Default | Description |
|---|---|---|
| `ISCHED_BUILD_DIR` | `cmake-build-debug` | CMake build directory used to locate `isched_srv` |
| `ISCHED_SERVER_PORT` | `18080` | Port the test server listens on |
| `ISCHED_EXTERNAL_SERVER` | _(unset)_ | Set to `1` to skip server launch (external harness manages it) |

## Additional Resources

For more information on using the Angular CLI, including detailed command references, visit the [Angular CLI Overview and Command Reference](https://angular.dev/tools/cli) page.
