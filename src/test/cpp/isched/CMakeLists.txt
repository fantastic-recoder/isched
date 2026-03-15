configure_file(hello_world_schema.graphql hello_world_schema.graphql COPYONLY)

add_executable(isched_grammar_tests isched_grammar_tests.cpp)
target_link_libraries(isched_grammar_tests PRIVATE taocpp::pegtl spdlog::spdlog isched Boost::url Catch2::Catch2WithMain)
add_test(NAME isched_grammar_tests COMMAND isched_grammar_tests)

add_executable(isched_graphql_tests isched_graphql_tests.cpp)
target_link_libraries(isched_graphql_tests PRIVATE isched Catch2::Catch2WithMain spdlog::spdlog nlohmann_json::nlohmann_json SQLite::SQLite3 taocpp::pegtl)
add_test(NAME isched_graphql_tests COMMAND isched_graphql_tests)

add_executable(isched_gql_executor_tests isched_gql_executor_tests.cpp isched_test_run_listener.cpp)
target_link_libraries(isched_gql_executor_tests PRIVATE isched Catch2::Catch2WithMain spdlog::spdlog nlohmann_json::nlohmann_json SQLite::SQLite3 taocpp::pegtl sago::platform_folders)
add_test(NAME isched_gql_executor_tests COMMAND isched_gql_executor_tests)

# Add the directory to include paths
include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_executable(isched_srv_test isched_srv_tests.cpp
        ../../../main/cpp/isched/backend/isched_LogEnvLoader.hpp)
target_link_libraries(isched_srv_test PRIVATE isched Boost::url Catch2::Catch2WithMain spdlog::spdlog
        httplib::httplib openssl::openssl nlohmann_json::nlohmann_json)
add_test(NAME isched_srv_test COMMAND isched_srv_test)

add_executable(basic_server_test ../../basic_server_test.cpp)
target_link_libraries(basic_server_test PRIVATE isched spdlog::spdlog  Boost::url)
add_test(NAME basic_server_test COMMAND basic_server_test)

add_executable(tenant_manager_test ../../tenant_manager_test.cpp)
target_link_libraries(tenant_manager_test PRIVATE isched spdlog::spdlog SQLite::SQLite3 nlohmann_json::nlohmann_json)
add_test(NAME tenant_manager_test COMMAND tenant_manager_test)

add_executable(isched_resolver_tests isched_resolver_tests.cpp)
target_link_libraries(isched_resolver_tests isched Boost::url Catch2::Catch2WithMain)
add_test(NAME isched_resolver_tests COMMAND isched_resolver_tests)

add_executable(database_test isched_database_test.cpp
        isched_test_run_listener.cpp
        ../../../main/cpp/isched/backend/isched_log_result.hpp)
target_link_libraries(database_test PRIVATE isched Catch2::Catch2WithMain SQLite::SQLite3 nlohmann_json::nlohmann_json spdlog::spdlog)
add_test(NAME database_test COMMAND database_test)

# New foundational component tests for Universal Application Server Backend
add_executable(isched_auth_tests isched_auth_tests.cpp)
target_link_libraries(isched_auth_tests PRIVATE isched Catch2::Catch2WithMain spdlog::spdlog
        nlohmann_json::nlohmann_json SQLite::SQLite3 sago::platform_folders taocpp::pegtl)
add_test(NAME isched_auth_tests COMMAND isched_auth_tests)

add_executable(isched_subscription_broker_tests isched_subscription_broker_tests.cpp)
target_link_libraries(isched_subscription_broker_tests PRIVATE isched Catch2::Catch2WithMain spdlog::spdlog nlohmann_json::nlohmann_json)
add_test(NAME isched_subscription_broker_tests COMMAND isched_subscription_broker_tests)

add_executable(isched_config_tests isched_config_tests.cpp isched_pattern_match_tests.cpp)
target_link_libraries(isched_config_tests PRIVATE isched Catch2::Catch2WithMain spdlog::spdlog)
add_test(NAME isched_config_tests COMMAND isched_config_tests)

# Integration tests for User Story 1 - Frontend Developer Setup
add_executable(test_server_startup ../integration/test_server_startup.cpp)
target_link_libraries(test_server_startup PRIVATE isched Catch2::Catch2WithMain Boost::url spdlog::spdlog nlohmann_json::nlohmann_json SQLite::SQLite3 Boost::url httplib::httplib openssl::openssl)
add_test(NAME test_server_startup COMMAND test_server_startup)

add_executable(test_builtin_schema ../integration/test_builtin_schema.cpp)
target_link_libraries(test_builtin_schema PRIVATE isched Catch2::Catch2WithMain spdlog::spdlog nlohmann_json::nlohmann_json httplib::httplib openssl::openssl Boost::url)
add_test(NAME test_builtin_schema COMMAND test_builtin_schema)

add_executable(test_health_queries ../integration/test_health_queries.cpp)
target_link_libraries(test_health_queries PRIVATE isched Catch2::Catch2WithMain spdlog::spdlog nlohmann_json::nlohmann_json httplib::httplib openssl::openssl Boost::url)
add_test(NAME test_health_queries COMMAND test_health_queries)

add_executable(isched_fs_utils_tests isched_fs_utils_tests.cpp)
target_link_libraries(isched_fs_utils_tests PRIVATE isched Catch2::Catch2WithMain spdlog::spdlog)
add_test(NAME isched_fs_utils_tests COMMAND isched_fs_utils_tests)

add_executable(isched_ast_node_tests isched_ast_node_tests.cpp)
target_link_libraries(isched_ast_node_tests PRIVATE isched Catch2::Catch2WithMain spdlog::spdlog
        Catch2::Catch2WithMain spdlog::spdlog nlohmann_json::nlohmann_json taocpp::pegtl )
add_test(NAME isched_ast_node_tests COMMAND isched_ast_node_tests)

add_executable(isched_multi_dim_map_tests isched_multi_dim_map_tests.cpp)
target_link_libraries(isched_multi_dim_map_tests PRIVATE Catch2::Catch2WithMain)
add_test(NAME isched_multi_dim_map_tests COMMAND isched_multi_dim_map_tests)

# Integration tests for User Story 2 - GraphQL-Native Configuration (T026-T028)
add_executable(test_configuration_snapshots ../integration/test_configuration_snapshots.cpp)
target_link_libraries(test_configuration_snapshots PRIVATE isched Catch2::Catch2WithMain
    spdlog::spdlog nlohmann_json::nlohmann_json httplib::httplib openssl::openssl
    Boost::url SQLite::SQLite3)
add_test(NAME test_configuration_snapshots COMMAND test_configuration_snapshots)

add_executable(test_schema_activation ../integration/test_schema_activation.cpp)
target_link_libraries(test_schema_activation PRIVATE isched Catch2::Catch2WithMain
    spdlog::spdlog nlohmann_json::nlohmann_json httplib::httplib openssl::openssl
    Boost::url SQLite::SQLite3)
add_test(NAME test_schema_activation COMMAND test_schema_activation)

add_executable(test_configuration_rollback ../integration/test_configuration_rollback.cpp)
target_link_libraries(test_configuration_rollback PRIVATE isched Catch2::Catch2WithMain
    spdlog::spdlog nlohmann_json::nlohmann_json httplib::httplib openssl::openssl
    Boost::url SQLite::SQLite3)
add_test(NAME test_configuration_rollback COMMAND test_configuration_rollback)

# Phase 5: WebSocket / Subscription integration tests (T037-T039)
set(_ws_common_libs isched Catch2::Catch2WithMain
    spdlog::spdlog nlohmann_json::nlohmann_json
    httplib::httplib openssl::openssl
    Boost::url Boost::system Boost::thread SQLite::SQLite3)

add_executable(test_graphql_websocket ../integration/test_graphql_websocket.cpp)
target_link_libraries(test_graphql_websocket PRIVATE ${_ws_common_libs})
add_test(NAME test_graphql_websocket COMMAND test_graphql_websocket)

add_executable(test_graphql_subscriptions ../integration/test_graphql_subscriptions.cpp)
target_link_libraries(test_graphql_subscriptions PRIVATE ${_ws_common_libs})
add_test(NAME test_graphql_subscriptions COMMAND test_graphql_subscriptions)

add_executable(test_client_compatibility ../integration/test_client_compatibility.cpp)
target_link_libraries(test_client_compatibility PRIVATE ${_ws_common_libs})
add_test(NAME test_client_compatibility COMMAND test_client_compatibility)

# Phase 6: User/Organization management integration tests (T047-018)
add_executable(test_user_management ../integration/test_user_management.cpp)
target_link_libraries(test_user_management PRIVATE isched Catch2::Catch2WithMain
    spdlog::spdlog nlohmann_json::nlohmann_json SQLite::SQLite3 sago::platform_folders
    taocpp::pegtl openssl::openssl)
add_test(NAME test_user_management COMMAND test_user_management)

# Phase 6b: Session revocation integration tests (T049-008)
add_executable(test_session_revocation ../integration/test_session_revocation.cpp)
target_link_libraries(test_session_revocation PRIVATE isched Catch2::Catch2WithMain
    spdlog::spdlog nlohmann_json::nlohmann_json SQLite::SQLite3 sago::platform_folders
    taocpp::pegtl openssl::openssl)
add_test(NAME test_session_revocation COMMAND test_session_revocation)

# T048: RestDataSource / CryptoUtils unit tests
add_executable(isched_rest_datasource_tests isched_rest_datasource_tests.cpp)
target_link_libraries(isched_rest_datasource_tests PRIVATE isched Catch2::Catch2WithMain
    spdlog::spdlog nlohmann_json::nlohmann_json SQLite::SQLite3 sago::platform_folders
    taocpp::pegtl httplib::httplib openssl::openssl)
add_test(NAME isched_rest_datasource_tests COMMAND isched_rest_datasource_tests)

# T050-005: Adaptive thread pool and subscription resource control unit tests
add_executable(isched_tenant_thread_pool_tests isched_tenant_thread_pool_tests.cpp)
target_link_libraries(isched_tenant_thread_pool_tests PRIVATE isched Catch2::Catch2WithMain
    spdlog::spdlog nlohmann_json::nlohmann_json SQLite::SQLite3 sago::platform_folders
    taocpp::pegtl httplib::httplib openssl::openssl)
add_test(NAME isched_tenant_thread_pool_tests COMMAND isched_tenant_thread_pool_tests)

# T051: Performance Metrics via GraphQL unit tests
add_executable(isched_metrics_tests isched_metrics_tests.cpp)
target_link_libraries(isched_metrics_tests PRIVATE isched Catch2::Catch2WithMain
    spdlog::spdlog nlohmann_json::nlohmann_json SQLite::SQLite3 sago::platform_folders
    taocpp::pegtl httplib::httplib openssl::openssl)
add_test(NAME isched_metrics_tests COMMAND isched_metrics_tests)

# T052: Performance Benchmark Suite
add_executable(benchmark_suite ../performance/benchmark_suite.cpp)
target_link_libraries(benchmark_suite PRIVATE isched Catch2::Catch2WithMain
    spdlog::spdlog nlohmann_json::nlohmann_json SQLite::SQLite3 sago::platform_folders
    taocpp::pegtl httplib::httplib openssl::openssl
    Boost::url Boost::system Boost::thread)
add_test(NAME benchmark_suite COMMAND benchmark_suite)

