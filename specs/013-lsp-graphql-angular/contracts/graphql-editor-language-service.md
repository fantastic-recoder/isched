# Contract: Browser GraphQL Editor Language Capabilities

**Scope**: User-visible capabilities of the Playground editor. This is not an external LSP/API contract. `UiFlowState` and `GraphQLErrorPresentation` remain internal frontend models described in `data-model.md`.

## Required Capabilities

- **Schema-aware completion**: Offer valid fields, arguments, types, enum values, and operation structure from the active GraphQL schema.
- **Hover documentation**: Show descriptions from the active schema for the symbol under the cursor/pointer. Missing descriptions remain absent; do not fabricate them.
- **Live diagnostics**: While editing, report syntax errors and schema/type validation errors with source locations and accessible UI presentation.
- **Schema lifecycle**: Obtain complete introspection including relevant descriptions through `/graphql`; construct/update the browser-local schema and editor before claiming language assistance is active.
- **Unavailable state**: If introspection fails or schema construction fails, show explicit language-service-unavailable feedback while preserving ordinary editing. Do not silently present empty results as successful validation.

## Boundary and Protocol

- Language service work runs in the browser with the existing CodeMirror/GraphQL stack.
- All server-derived data uses the existing GraphQL `/graphql` endpoint. No REST, direct backend URL, separate external LSP endpoint, or additional application transport is allowed.
- **Actual LSP JSON-RPC wire-level conformance is not required.** Implement the specified editor capabilities locally; do not plan a remote LSP server or protocol adapter solely to satisfy the feature name.

## Verification

- Automated editor tests prove schema-based completion, hover descriptions, syntax errors, type/schema errors, and unavailable-schema behavior.
- Network/boundary tests prove editor language support creates no external request outside GraphQL `/graphql` introspection.
- Angular strict TypeScript/template checks pass; any new components remain standalone, use app-owned signals, typed forms where user input exists, and modern control flow.
