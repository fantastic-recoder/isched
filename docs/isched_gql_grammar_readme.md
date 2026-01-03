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
The changes were verified by adding and running unit tests in:
- `src/test/cpp/isched/isched_grammar_tests.cpp`: Focused on grammar compliance.
- `src/test/cpp/isched/isched_gql_executor_tests.cpp`: Focused on AST structure and description capture.

### Test Case Tagging Scheme
The unit tests use a systematic tagging scheme to categorize and filter tests:
- `[grammar][lexical][positive/negative]`: Basic tokens and lexical rules (Int, Float, String, Name, etc.).
- `[grammar][type-system][positive/negative]`: Type system definitions and documents.
- `[grammar][executable][positive/negative]`: Executable documents (Queries, Mutations, Selections).
- `[grammar][spec-examples][positive]`: Tests derived from official GraphQL specification examples.
- `[grammar][internal][positive/negative]`: Internal parser helper rules.
- `[grammar][lexical/type-system][integration]`: Tests verifying interaction between multiple rules.
- `[gql][ast][descriptions]`: Verification of descriptions in the generated AST.

Tags are consistently ordered as `[grammar][category][outcome/integration]` or `[gql][category][feature]`.

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
- **Nested type definitions**: Verified that multiple object types can reference each other (e.g., `User` referencing `Profile`).
- **Nested types in fields**: Verified character-perfect reconstruction of complex types like `[User!]` within field definitions using `ast_node_to_str`.

### Grammar Improvements
To support these examples, the following improvements were made to `isched_gql_grammar.hpp`:
- **`TSeparator`**: Updated to include `Comma` (`,`), ensuring that insignificant commas are correctly ignored between tokens.
- **`Description`**: Added support for capture in the AST for various constructs (types, fields, scalars, schema, operations, arguments).
- **`ArgumentsDefinition`**: Added to support formal argument lists in type system definitions (e.g., in `FieldDefinition`).
- **`FieldDefinition`**: Updated to use `ArgumentsDefinition` instead of executable `Arguments`, allowing descriptions on field arguments in the type system.
- **AST Selector**: Updated to include `ArgumentsDefinition`, `InputValueDefinition`, and `Description` for better parse tree representation.

## AST to String Conversion

### Overview
The function `ast_node_to_str(const TAstNodePtr &p_node)` was implemented to allow converting an AST node back to its original string representation. This is particularly useful for debugging, logging, and potentially for schema merging or transformation tasks where the original formatting (including comments and whitespaces) needs to be preserved.

### Implementation Details
- **File**: `src/main/cpp/isched/backend/isched_gql_grammar.cpp`
- **Mechanism**: The function recursively traverses the AST.
    - If a node `has_content()` (as defined in `GqlSelector`), it returns the `string_view()` of that node.
    - If a node does not have content, it concatenates the results of calling `ast_node_to_str` on all its children.
- **Formatting Preservation**: Because the grammar includes rules that capture whitespace and comments (e.g., `Document`, `OperationDefinition`, `FieldDefinition` use `TSeps` or `IgnoredMany`), and these rules are included in `GqlSelector`, the resulting string is a character-perfect reconstruction of the input in most cases.

## AST Merging

### Overview
The utility `merge_type_definitions(TAstNodePtr &&p_schema_node, TAstNodePtr &&p_type_defs_node)` allows merging multiple GraphQL documents into a single AST.

### Implementation Details
- **File**: `src/main/cpp/isched/backend/isched_gql_grammar.cpp`
- **Mechanism**: The function appends children from the second document node to the first one, effectively combining the type definitions.

### Verification
The implementation is verified in `src/test/cpp/isched/isched_ast_node_tests.cpp`.

#### Test Cases
- **Schema with description**: Reconstructing a schema definition starting with a block string description.
- **Simple Query**: Reconstructing basic selection sets.
- **Complex Mutation**: Reconstructing mutations with nested fields and block string arguments.
- **Type System Definitions**: Reconstructing `scalar` and `type` definitions with fields and descriptions.
- **Directives and Arguments**: Reconstructing operations and fields with directives and arguments.
- **AST Merging**: Verifying that multiple document ASTs can be merged correctly (`[isched][ast][merge]`).

## File Locations
- Grammar Header: `src/main/cpp/isched/backend/isched_gql_grammar.hpp`
- Unit Tests:
    - `src/test/cpp/isched/isched_grammar_tests.cpp`: Grammar compliance.
    - `src/test/cpp/isched/isched_gql_executor_tests.cpp`: AST Descriptions.
    - `src/test/cpp/isched/isched_ast_node_tests.cpp`: AST to String and Merging.
- Specification: `docs/graph-ql-spec/GraphQL.html`
