# Quickstart: LSP GraphQL Angular Integration

## Prerequisites
- Backend service running with GraphQL endpoint at `/graphql`
- Angular WebUI dependencies installed

## Local Run
1. Start backend service.
2. Start Angular app with configured dev proxy for `/graphql` (HTTP + WS).
3. Open UI and execute primary feature flow.

## Validation Checklist
- Primary flow succeeds via GraphQL operations only.
- Recoverable GraphQL errors show actionable UI feedback with retry.
- No JWT persisted in localStorage/sessionStorage/IndexedDB.
- Subscription-dependent behavior works through proxy WS upgrade.
- Relevant frontend/backend/integration tests pass.
