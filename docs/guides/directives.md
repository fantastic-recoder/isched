# Using Directives in GraphQL

Directives provide a way to describe additional configuration to the executor. They can be used to change the result of a query, or to provide metadata about the schema.

## Built-in Directives

GraphQL spec defines several built-in directives:

### `@skip(if: Boolean!)`
The `@skip` directive may be provided for fields, fragment spreads, and inline fragments, and allows for conditional exclusion during execution as described by the `if` argument.

```graphql
query myQuery($shouldSkip: Boolean!) {
  field @skip(if: $shouldSkip)
}
```

### `@include(if: Boolean!)`
The `@include` directive may be provided for fields, fragment spreads, and inline fragments, and allows for conditional inclusion during execution as described by the `if` argument.

```graphql
query myQuery($shouldInclude: Boolean!) {
  field @include(if: $shouldInclude)
}
```

### `@deprecated(reason: String)`
The `@deprecated` directive is used within the type system definition language to indicate that a field or enum value is deprecated.

```graphql
type MyType {
  oldField: String @deprecated(reason: "Use newField instead")
  newField: String
}
```

## Custom Directives

You can also define your own custom directives in the schema.

### Defining a Directive

```graphql
directive @auth(role: String) on FIELD_DEFINITION
```

### Using a Custom Directive

```graphql
type Query {
  secretData: String @auth(role: "ADMIN")
}
```

## Introspection

You can query the available directives using introspection:

```graphql
query {
  __schema {
    directives {
      name
      description
      locations
      args {
        name
        type {
          name
        }
      }
    }
  }
}
```
