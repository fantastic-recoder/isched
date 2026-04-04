# Security Threat Model Summary

**Updated**: 2026-04-04

## Purpose

This document summarizes the security posture and recurring mitigations for Isched security-sensitive features. Feature-specific threat analysis remains in each feature directory and is referenced here.

## Feature Threat Models

- `specs/001-universal-backend/threat-model.md` — GraphQL transport, bootstrap flow, JWT auth, RBAC, tenant isolation, session revocation, WebSocket auth, and outbound HTTP secret handling

## Common Security Themes

- **Secure bootstrap**: any unauthenticated bootstrap path must be narrow, explicitly documented, and automatically disabled after first-use conditions are satisfied.
- **JWT-first authentication**: GraphQL operations require JWT validation, with scope-aware authorization for platform and tenant operations.
- **RBAC with scoped roles**: platform and tenant permissions must remain separated; custom roles must be constrained to their owning scope.
- **Tenant isolation**: tenant boundaries apply to authorization, runtime state, storage, metrics, and integration configuration.
- **Session revocation**: revoked sessions must be enforced at request time and propagated to long-lived WebSocket connections.
- **Secret protection**: external integration secrets must not be stored in plaintext and must be protected against accidental disclosure in logs or responses.

## Reusable Mitigation Checklist

- validate authentication before resolver execution
- enforce platform-vs-tenant scope explicitly in authorization checks
- persist and check revocation state for active sessions
- encrypt stored secrets at rest
- document trust boundaries and residual risks for every security-sensitive feature
- include threat-model updates when auth, RBAC, session, transport, or secret-handling behavior changes

## Operational Follow-Ups

- maintain signing-key rotation guidance
- review logging for secret redaction and safe error reporting
- review denial-of-service protections for GraphQL queries, subscriptions, and outbound integrations
- re-run threat-model review when adding new authentication flows or privileged mutations

