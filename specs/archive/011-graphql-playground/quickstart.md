# Quickstart: GraphQL Playground

## Prerequisites

- Working checkout of the repository
- Node.js environment suitable for the Angular workspace
- Backend GraphQL server available through the existing dev proxy

## Local development

1. Start the UI:
   ```bash
   cd /home/groby/dev/isched/src/ui
   pnpm start
   ```
2. Ensure the backend is available on the proxied `/graphql` target used by the Angular dev server.
3. Open the app and navigate to `/playground` as an authenticated user.

## Validation steps

### Unit tests

Run the Angular test suite, or the focused playground tests once they exist:

```bash
cd /home/groby/dev/isched/src/ui
pnpm test:ci
```

### End-to-end tests

Run Playwright to verify the key user flows:

```bash
cd /home/groby/dev/isched/src/ui
pnpm e2e
```

## Manual verification checklist

- `/playground` is accessible only after authentication.
- The left tree loads the full introspection graph and merged uploaded schema documents.
- Selecting a field enables Generate Query.
- Generate Query inserts a valid stub into the CodeMirror 6 editor.
- Run shows loading, success, error, or advisory state as appropriate.
- Left/right and top/bottom splitter sizes persist after a page refresh.

## Implementation notes

- Keep all GraphQL traffic on relative `/graphql`.
- Do not persist credentials in browser storage.
- Store only layout preferences in browser-persistent UI state.

