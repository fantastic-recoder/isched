# GraphQL Grammar Protocol

## Overview
This document describes the changes made to the GraphQL grammar in `isched_gql_grammar.hpp` to align it with the official GraphQL specification as documented in `docs/graph-ql-spec/GraphQL.html`.

## Renamed Rules
The following rules were renamed for spec compliance:

- `Ws` -> `WhitespaceOnly`
- `IntValueCore` -> `IntegerPart`
- `NameContinues` -> `NameContinue`
- `StringType` -> `String`
- `GqlTypeInt` -> `Int`
- `GqlTypeFloat` -> `Float`
- `GqlTypeBoolean` -> `Boolean`
- `GqlTypeID` -> `ID`
- `GqlTypeField` -> `FieldDefinition`
- `GqlTypeFields` -> `FieldsDefinition`

## Documentation
Every rule in `isched_gql_grammar.hpp` has been documented with references to the corresponding section in the GraphQL specification using `@see GraphQL Spec: "RuleName"`.

## Verification
The changes were verified by adding and running unit tests in `src/test/cpp/isched/isched_grammar_tests.cpp`.

### Test Case Tagging Scheme
The unit tests use a systematic tagging scheme to categorize and filter tests:
- `[grammar][lexical][positive/negative]`: Basic tokens and lexical rules (Int, Float, String, Name, etc.).
- `[grammar][type-system][positive/negative]`: Type system definitions and documents.
- `[grammar][executable][positive/negative]`: Executable documents (Queries, Mutations, Selections).
- `[grammar][spec-examples][positive]`: Tests derived from official GraphQL specification examples.
- `[grammar][internal][positive/negative]`: Internal parser helper rules.
- `[grammar][lexical/type-system][integration]`: Tests verifying interaction between multiple rules.

Tags are consistently ordered as `[grammar][category][outcome/integration]`.

### Added Specification Examples
The following examples from `docs/graph-ql-spec/GraphQL.html` were added as unit tests:
- **Example № 1**: Simple query.
- **Example № 5**: Query shorthand.
- **Example № 9**: Query with arguments.
- **Example № 10**: Query with multiple arguments (verified handling of insignificant commas).
- **Example № 17**: Complex query with nested fields.
- **Example № 24**: Mutation using block strings.
- **Example № 26**: Standalone block string.
- **Example № 35**: Schema and Type definitions with block string descriptions and argument definitions.
- **Example № 39**: Schema definition with multiple root operation types.
- **Example № 43**: Multiple scalar type definitions with directives.

### Grammar Improvements
To support these examples, the following improvements were made to `isched_gql_grammar.hpp`:
- **`TSeparator`**: Updated to include `Comma` (`,`), ensuring that insignificant commas are correctly ignored between tokens.
- **`ArgumentsDefinition`**: Added to support formal argument lists in type system definitions (e.g., in `FieldDefinition`).
- **`FieldDefinition`**: Updated to use `ArgumentsDefinition` instead of executable `Arguments`, allowing descriptions on field arguments in the type system.
- **AST Selector**: Updated to include `ArgumentsDefinition` and `InputValueDefinition` for better parse tree representation.

## File Locations
- Grammar Header: `src/main/cpp/isched/backend/isched_gql_grammar.hpp`
- Unit Tests: `src/test/cpp/isched/isched_grammar_tests.cpp`
- Specification: `docs/graph-ql-spec/GraphQL.html`
