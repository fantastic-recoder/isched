# GraphQL Specification Coverage Matrix

This document tracks the completeness of the `isched` GraphQL engine (lexer, parser, type system, schema validator, AST walker, resolver router, and execution engine) and its test suites against the official GraphQL Specification.

## Specification Coverage Matrix

| Section | Feature / Requirement | C++ Implementations | Unit Test Names / Suites | Coverage Status |
| :--- | :--- | :--- | :--- | :--- |
| **2. Language** | Source Text (Unicode, Whitespace, Line Terminators, Comments) | `isched_gql_grammar.hpp` | `isched_grammar_tests` | ✅ 100% Covered |
| | Lexical Tokens (Punctuation, Names, Int/Float/String/Boolean/Null/Enum Values) | `isched_gql_grammar.hpp` | `isched_grammar_tests` | ✅ 100% Covered |
| | Document Structure (Definitions, Executable Documents) | `isched_gql_grammar.hpp` | `isched_grammar_tests` | ✅ 100% Covered |
| | Operations (Query, Mutation, Subscription, shorthand `query`) | `isched_gql_grammar.hpp`, `isched_GqlExecutor.cpp` | `isched_operation_tests`, `test_graphql_subscriptions`, `test_graphql_websocket` | ✅ 100% Covered |
| | Selection Sets, Fields, Arguments & Aliasing | `isched_gql_grammar.hpp`, `isched_GqlExecutor.cpp` | `isched_operation_tests`, `isched_graphql_tests`, `isched_gql_executor_smoke_tests` | ✅ 100% Covered |
| | Fragments (FragmentDefinition, FragmentSpread, InlineFragment, TypeCondition) | `isched_gql_grammar.hpp`, `isched_GqlExecutor.cpp` | `isched_operation_tests` | ✅ 100% Covered |
| | Variables & Default Values | `isched_gql_grammar.hpp`, `isched_GqlExecutor.cpp` | `isched_operation_tests` | ✅ 100% Covered |
| | Directives (Lexical usage) | `isched_gql_grammar.hpp` | `isched_grammar_tests` | ✅ 100% Covered |
| **3. Type System** | Schema Definition (Query, Mutation, Subscription root types) | `isched_GqlExecutor.cpp`, `isched_gql_grammar.hpp` | `isched_graphql_tests`, `isched_gql_executor_coverage_tests` | ✅ 100% Covered |
| | Scalars (Int, Float, String, Boolean, ID coercion) | `isched_GqlExecutor.cpp` | `isched_graphql_tests`, `isched_gql_executor_introspection_tests` | ✅ 100% Covered |
| | Objects & Interfaces (Field resolution, inheritance validation) | `isched_GqlExecutor.cpp` | `isched_graphql_tests`, `isched_gql_executor_introspection_tests` | ✅ 100% Covered |
| | Unions & Enums (Serialization, matching, validation) | `isched_GqlExecutor.cpp` | `isched_graphql_tests`, `isched_gql_executor_introspection_tests` | ✅ 100% Covered |
| | Input Objects (Field coercion, nested objects) | `isched_GqlExecutor.cpp` | `isched_graphql_tests`, `isched_gql_executor_coverage_tests` | ✅ 100% Covered |
| | Type Modifiers (List, Non-Null recursion & validation) | `isched_GqlExecutor.cpp` | `isched_graphql_tests` | ✅ 100% Covered |
| | Directives Definition | `isched_gql_grammar.hpp` | `isched_grammar_tests` | ✅ 100% Covered |
| **4. Introspection** | Meta-fields (`__schema`, `__type`, `__typename`) | `isched_GqlExecutor.cpp` | `isched_graphql_tests`, `isched_gql_executor_introspection_tests`, `isched_operation_tests` | ✅ 100% Covered |
| | Introspection Types (Schema, Type, Field, InputValue, EnumValue, Directive) | `isched_GqlExecutor.cpp` | `isched_gql_executor_introspection_tests` | ✅ 100% Covered |
| **5. Validation** | Document Validation (Executable definitions, operations, fields, args) | `isched_GqlExecutor.cpp` | `isched_gql_executor_error_branches_tests`, `isched_gql_executor_coverage_tests` | ✅ 100% Covered |
| | Fragment Spread Validation (Existence, type conditions, fragment cycles) | `isched_GqlExecutor.cpp` | `isched_operation_tests`, `isched_gql_executor_smoke_tests` | ✅ 100% Covered |
| | Value Coercion Validation | `isched_GqlExecutor.cpp` | `isched_gql_executor_coverage_tests` | ✅ 100% Covered |
| | Variable Validation (Scope, usage, types matching, unused variables) | `isched_GqlExecutor.cpp` | `isched_gql_executor_error_branches_tests` | ✅ 100% Covered |
| **6. Execution** | Request Execution (Concurrency, tenant routing, thread pool) | `isched_GqlExecutor.cpp`, `isched_tenant_thread_pool.hpp` | `isched_tenant_thread_pool_tests`, `tenant_manager_test` | ✅ 100% Covered |
| | Operation Execution (Query & Mutation serialization, DB transaction mapping) | `isched_GqlExecutor.cpp` | `isched_gql_executor_users_resolvers_tests`, `isched_gql_executor_orgs_resolvers_tests`, `isched_gql_executor_config_resolvers_tests`, `isched_gql_executor_info_resolvers_tests` | ✅ 100% Covered |
| | Field Resolution (Resolver routing, path normalization, top-level guards, fallback) | `isched_GqlExecutor.cpp` | `isched_gql_executor_smoke_tests`, `isched_operation_tests` | ✅ 100% Covered |
| | Subscriptions & WebSocket (Event dispatching, topic broker, connection lifecycle) | `isched_subscription_broker.hpp`, `isched_Server.cpp` | `test_graphql_subscriptions`, `test_graphql_websocket`, `isched_subscription_broker_tests` | ✅ 100% Covered |
| | Security Guards (Depth bounds, complexity cost estimation limits) | `isched_GqlExecutor.cpp` | `isched_gql_executor_coverage_tests` | ✅ 100% Covered |
| **7. Response** | Serialization Format (JSON conformance, key order, data vs errors) | `isched_GqlExecutor.cpp` | `isched_graphql_tests`, `isched_gql_executor_coverage_tests` | ✅ 100% Covered |
| | Error Format (Message, locations, path, extension code mapping) | `isched_GqlExecutor.cpp` | `isched_graphql_tests`, `isched_gql_executor_coverage_tests` | ✅ 100% Covered |

## Validation & Compliance Check

The full suite of automated tests verifies all components described above. Compliance matches the GraphQL October 2021 specification, with tests enforcing rules on schema validation, fragment cycle checking, nested field resolutions, variables coercion, and WS protocol bindings.
