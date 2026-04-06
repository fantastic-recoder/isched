# Threat Model: Tenant Admin Schema Upload

**Feature**: `006-upload-schema`
**Updated**: 2026-04-06
**Scope**: GraphQL schema document upload, list, and fetch endpoints

---

## Threat 1: Unauthorized schema upload (Elevation of Privilege)

**Description**: Non-admin or unauthenticated caller attempts to upload or overwrite a schema document.

**Controls**:
- `uploadSchemaDocument` gated by `require_roles({"role_tenant_admin"})`.
- Callers without `role_tenant_admin` receive `FORBIDDEN` before resolver executes.
- Unauthenticated callers (empty roles) are rejected by the RBAC gate.

**Residual Risk**: Low.

---

## Threat 2: Cross-tenant data leakage (Information Disclosure)

**Description**: A tenant member requests schema documents belonging to another tenant.

**Controls**:
- All operations derive tenant scope exclusively from `ResolverCtx.tenant_id` (JWT-populated).
- No caller-supplied `organizationId` argument accepted.
- Per-tenant SQLite DBs enforce physical isolation.

**Residual Risk**: Negligible.

---

## Threat 3: Destructive overwrite without intent (Tampering)

**Description**: Admin accidentally or maliciously overwrites an existing schema document.

**Controls**:
- Default `overwrite=false`; conflict returned when name already exists.
- Explicit `overwrite=true` required to replace.
- `updatedBy` + `updatedAt` recorded atomically for audit.

**Residual Risk**: Low.

---

## Threat 4: Concurrent overwrite race (Tampering)

**Description**: Two concurrent `overwrite=true` uploads produce partial or inconsistent state.

**Controls**:
- `replace_schema_document()` uses `BEGIN IMMEDIATE` transaction.
- SQLite WAL mode; last-successful-write-wins by commit order.

**Residual Risk**: Low.

---

## Threat 5: Malformed SDL injection (Tampering / DoS)

**Description**: Admin uploads malformed or adversarially crafted GraphQL SDL.

**Controls**:
- PEGTL SDL parser validates content before persistence.
- Malformed SDL returns `VALIDATION_FAILED` without DB write.

**Residual Risk**: Low.

---

## Threat 6: Oversized document DoS

**Description**: Admin uploads very large SDL to exhaust memory or disk.

**Controls**:
- Size checked against configurable max (default 1 MB) before parsing.
- Oversized documents return `VALIDATION_FAILED` immediately.

**Residual Risk**: Low.

---

## Threat 7: Schema name injection (Tampering)

**Description**: Admin supplies schema name with SQL injection or path traversal chars.

**Controls**:
- Name validated against `[A-Za-z0-9._-]{1,128}` before any DB interaction.
- All SQL uses parameterized `?` binding.

**Residual Risk**: Negligible.

---

## Summary

| Threat | Severity | Control | Residual |
|--------|----------|---------|----------|
| Unauthorized upload | High | RBAC gate | Low |
| Cross-tenant leakage | High | Auth-ctx tenant scope | Negligible |
| Destructive overwrite | Medium | Explicit flag | Low |
| Concurrent write race | Medium | IMMEDIATE transaction | Low |
| Malformed SDL | Medium | PEGTL parser | Low |
| Oversized doc DoS | Medium | Max-size check | Low |
| Name injection | Medium | Regex + parameterized SQL | Negligible |
