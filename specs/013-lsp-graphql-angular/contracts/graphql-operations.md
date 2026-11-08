# Contract: GraphQL Operations

## Endpoint
- `/graphql` (HTTP for query/mutation, WebSocket for subscription)

## Primary Flow Operations
- Define one query/mutation pair (and optional subscription) matching the feature journey in spec scenarios.
- Operation documents MUST be typed and validated against server schema.

## Error Contract
- GraphQL errors are mapped into UI categories:
  - `validation`
  - `authorization`
  - `transport`
  - `unknown`
- UI must show actionable feedback and preserve entered form data for recoverable failures.
