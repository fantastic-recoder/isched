# Threat Model: Universal Application Server Backend

**Feature**: `001-universal-backend`  
**Created**: 2026-04-04  
**Status**: Draft  
**Related Spec**: `specs/001-universal-backend/spec.md`

## Scope

This threat model covers the security-sensitive behavior defined for the Universal Application Server Backend, with emphasis on:

- one-time bootstrap and initial platform-admin provisioning
- JWT-based authentication for GraphQL over HTTP and WebSocket
- RBAC with platform-scoped roles, tenant-scoped roles, and custom scoped roles
- tenant isolation across runtime state, databases, and authorization boundaries
- session creation, revocation, and forced WebSocket termination
- outbound HTTP integrations and secret handling

## Assets

- platform admin accounts and credentials
- tenant admin accounts and user credentials
- JWT signing keys and validation configuration
- session records and revocation state
- tenant SQLite databases and `isched_system.db`
- stored API keys and other outbound integration secrets
- GraphQL schemas, resolver definitions, and access-control metadata

## Actors

- unauthenticated client
- authenticated platform admin
- authenticated tenant admin
- authenticated tenant user
- malicious tenant attempting cross-tenant access
- external upstream service used by outbound HTTP integrations

## Entry Points

- GraphQL HTTP endpoint at `/graphql`
- GraphQL WebSocket endpoint at `/graphql`
- bootstrap/admin setup operation available only before initial platform-admin provisioning
- configuration mutations affecting schema, auth, and resolver behavior
- outbound HTTP resolver execution

## Trust Boundaries

1. External client ↔ GraphQL transport boundary
2. Authenticated request context ↔ RBAC authorization layer
3. Platform scope ↔ tenant scope
4. Application runtime ↔ SQLite storage (`isched_system.db` and tenant DBs)
5. Application runtime ↔ external upstream HTTP services

## Threat Scenarios and Mitigations

| Threat | Impact | Mitigations | Residual Risk |
|---|---|---|---|
| Reuse or abuse of bootstrap flow after initial setup | Unauthorized platform takeover | Bootstrap allowed only once globally; disabled immediately after initial platform-admin provisioning; explicit rejection path required | Misconfiguration risk if bootstrap state is not persisted correctly |
| Forged, expired, or replayed JWTs | Unauthorized access | JWT validation on each HTTP request and WebSocket operation; session validation and revocation checks; secure signing-key management | Key compromise remains high impact |
| Cross-tenant privilege escalation | Tenant data exposure or unauthorized admin actions | Scope-aware RBAC, tenant claim enforcement, tenant-scoped DB separation, platform/tenant operation checks | Resolver bugs or missing guards could still create escapes |
| Misuse of custom roles in schema-defined access control | Over-broad access grants | Scoped role definitions, schema validation requiring referenced roles to exist, documentation of role semantics | Human error in role design remains possible |
| Stale sessions after revocation | Continued unauthorized access | Persisted revocation state, per-request token validation, forced WebSocket termination on revocation | Small race window may remain between revocation and enforcement |
| Secret disclosure in outbound HTTP integrations | Credential compromise | Encrypt stored API keys, derive tenant-specific encryption material, avoid plaintext persistence, restrict tenant access | Memory disclosure or logging mistakes remain a concern |
| WebSocket authentication bypass | Unauthorized subscriptions and event leakage | Authenticate at connection setup and per operation as required, enforce tenant scope before subscription registration | Long-lived connection handling remains sensitive to state bugs |

## Security Assumptions

- OpenSSL-based crypto primitives are correctly configured and kept up to date.
- JWT signing secrets or keys are stored securely outside the repository.
- SQLite file permissions are configured to prevent unauthorized host-level access.
- Platform admins and tenant admins follow least-privilege practices when defining custom roles.

## Residual Risks Requiring Ongoing Review

- signing-key rotation procedures and operational rollout
- auditability of custom role changes and schema access-control changes
- denial-of-service risks via expensive queries, subscriptions, or external HTTP calls
- secure secret redaction in logs and error payloads

## Related Project Summary

See `docs/security-threat-model.md` for the project-wide security summary and reusable mitigations.
