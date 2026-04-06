# Data Model: WebUI Navigation + Status Bars

**Phase**: 1 (Design & Contracts)  
**Updated**: 2026-04-06  
**Feature**: `007-webui-nav-status-bars`

## Entity: NavigationItem

**Purpose**: Represents one top-bar menu destination for authenticated users.

**Fields**:
- `id: string` (stable key, e.g., `dashboard`, `admin-organizations`)
- `label: string` (user-facing text)
- `route: string` (Angular router link)
- `icon?: string` (optional icon token if used)
- `active: boolean` (derived from current route)
- `visible: boolean` (role/context-based display gate)

**Validation Rules**:
- `label` MUST be non-empty and human readable.
- `route` MUST map to an existing authenticated route.
- Exactly one primary item SHOULD be active for a concrete page route.
- Hidden items (`visible=false`) MUST not consume navigation space.

## Entity: OperationDigest

**Purpose**: Latest user-facing summary of the most recent tracked operation in the shell status bar.

**Fields**:
- `message: string`
- `state: 'idle' | 'loading' | 'success' | 'error'`
- `operationKey: string` (stable key for flow, e.g., `organization-users:list`)
- `updatedAt: string` (ISO timestamp)
- `sequence: number` (monotonic sequence for latest-wins semantics)

**Validation Rules**:
- `message` MUST be non-empty.
- Representative organization-user flow MUST include:
  - loading message: `Loading organization users`
  - success message: `Organization users loaded`
- When digests arrive rapidly, only highest `sequence` is rendered.
- `state='error'` messages MUST be end-user understandable.

## Entity: SessionIdentitySummary

**Purpose**: Lightweight shell identity shown in the bottom status bar.

**Fields**:
- `displayName: string`
- `userId?: string`
- `resolved: boolean`
- `fallbackLabel: string` (default non-empty label while unresolved)

**Validation Rules**:
- Shell identity text MUST never be empty in UI.
- If identity is unresolved, `fallbackLabel` MUST be shown.
- On resolution, `displayName` MUST replace fallback automatically without page reload.

## Entity: ShellViewModel

**Purpose**: Aggregated signal-backed state consumed by shell template.

**Fields**:
- `logoAssetPath: string` (e.g., `assets/isched_logo.jpg`)
- `navigation: NavigationItem[]`
- `operationDigest: OperationDigest`
- `identity: SessionIdentitySummary`
- `authenticatedShellVisible: boolean`

**Validation Rules**:
- `logoAssetPath` MUST resolve to an existing WebUI assets file.
- `authenticatedShellVisible` is true only for authenticated application screens in feature scope.
- Shell layout MUST keep `operationDigest` and `identity` simultaneously readable across supported viewports.

## Relationships

- `ShellViewModel.navigation` is a collection of `NavigationItem`.
- `ShellViewModel.operationDigest` is the current `OperationDigest` snapshot.
- `ShellViewModel.identity` is the current `SessionIdentitySummary` snapshot.
- `OperationDigest` updates originate from tracked operation producers (initially organization-user fetch flow, extendable to other operations).

## State Transitions

### OperationDigest lifecycle

- `idle -> loading`: tracked operation starts.
- `loading -> success`: operation completes successfully.
- `loading -> error`: operation fails.
- `success/error -> loading`: a newer tracked operation starts.
- Any prior state -> newer state with higher `sequence`: newest digest wins.

### SessionIdentitySummary lifecycle

- `unresolved(fallback)` -> `resolved(displayName)` when current user identity becomes available.
- `resolved(displayName)` -> `unresolved(fallback)` on sign-out/session expiry.

## Derived Invariants

- Authenticated screens always render both top navigation and bottom status bar.
- Operation digest reflects the most recent tracked operation, not stale historical text.
- Current user area always contains a non-empty label.
- Navigation active state remains consistent with Angular router location.

