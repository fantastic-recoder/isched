# Contract: Auth Session Handling

## Browser Storage Rules
- JWT/access tokens MUST NOT be persisted in localStorage, sessionStorage, or IndexedDB.
- Auth context is in-memory for active session runtime.

## Authorization Behavior
- Protected GraphQL operations must include valid auth context.
- On auth expiry/invalid session:
  - Operation fails gracefully.
  - UI transitions to re-authentication path.
  - Sensitive in-memory auth material is cleared.
