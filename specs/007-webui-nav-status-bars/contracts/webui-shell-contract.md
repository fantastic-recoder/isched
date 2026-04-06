# WebUI Contract: Authenticated Shell Navigation and Status Bars

**Feature**: `007-webui-nav-status-bars`  
**Surface**: Angular WebUI authenticated shell (`src/ui/src/app`)

## Contract Goals

- Provide a consistent top navigation bar with isched asset logo and menu across authenticated screens.
- Provide a persistent bottom status bar that shows latest operation digest and current user name.
- Keep behavior signal-first, GraphQL-only for data sourcing, and testable in unit + smoke E2E flows.

## UI Contract Scope

### 1) Top Navigation Bar

**Required elements**:
- Brand/logo image sourced from WebUI assets directory (default plan path: `assets/isched_logo.jpg`).
- Navigation menu entries for primary authenticated destinations:
  - `Dashboard` -> `/dashboard`
  - `Organizations` -> `/admin/organizations`
  - `Users` -> `/admin/users`
  - `RBAC` -> `/admin/rbac`
- Active destination indication using clear visual state (`btn-active`, `tab-active`, or equivalent DaisyUI-consistent class pattern).

**Behavioral contract**:
- Top nav is visible on all authenticated screens in feature scope.
- Selecting a menu item navigates to the destination route.
- Current route is visually identifiable as active.

### 2) Bottom Status Bar

**Required elements**:
- Operation digest region showing latest tracked operation message.
- Current-user region showing non-empty user identity label.

**Behavioral contract**:
- Bottom status bar is visible on all authenticated screens in feature scope.
- Representative organization-user flow must transition digest text:
  - on start -> `Loading organization users`
  - on success -> `Organization users loaded`
- On operation failure, digest changes to understandable failure-oriented wording.
- If multiple digest updates occur rapidly, newest update remains displayed (latest-wins semantics).
- If identity is temporarily unavailable, a non-empty fallback label is shown and later replaced by resolved user name.

### 3) Responsive/Accessibility Contract

- Navigation and status bars remain readable across supported viewport sizes.
- Long digest text must not overlap identity text.
- Status digest region uses polite live-region semantics for update announcements when applicable.
- Interactive navigation elements are keyboard reachable and preserve visible focus states.

## Data and Transport Contract

### Data sources

- Identity data comes from existing authenticated GraphQL session context (via existing frontend auth/session services).
- Operation digest updates are emitted from frontend operation flows (initial tracked flow: organization user fetch).

### Transport rules

- All backend calls remain through `/graphql` only.
- No REST endpoints, no alternate transport introduction.
- No persistent browser token storage introduced (`localStorage`/`sessionStorage`/IndexedDB remain unused for auth tokens).

## Testing Contract

### Unit/component expectations

- Root shell render test verifies top nav and bottom status presence on authenticated routes.
- Nav menu tests verify route navigation and active-state indication.
- Shell status service/component tests verify digest lifecycle transitions (`loading`, `success`, `error`) and latest-wins update ordering.
- Identity tests verify fallback label behavior and resolved-name replacement.

### Playwright smoke expectations

- Smoke test verifies logo visibility and top nav menu on authenticated path.
- Smoke test verifies bottom status bar contains digest and current user label.
- Smoke test verifies representative digest transition from loading to success during organization-user fetch path.

## Compatibility Rules

- Existing GraphQL operation contracts remain backward compatible; this feature composes existing session/operation data.
- Existing auth/bootstrap behavior remains intact.
- App shell uses standalone Angular component conventions with separate `.html` and `.scss` files.

