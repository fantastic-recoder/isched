add_executable(isched_grammar_tests isched_grammar_tests.cpp)
target_link_libraries(isched_grammar_tests PRIVATE taocpp::pegtl restbed::restbed spdlog::spdlog isched Catch2::Catch2WithMain)
add_test(NAME isched_grammar_tests COMMAND isched_grammar_tests)

add_executable(isched_graphql_tests isched_graphql_tests.cpp)
target_link_libraries(isched_graphql_tests PRIVATE isched Catch2::Catch2WithMain spdlog::spdlog nlohmann_json::nlohmann_json SQLite::SQLite3)
add_test(NAME isched_graphql_tests COMMAND isched_graphql_tests)

add_executable(isched_srv_test isched_srv_tests.cpp
        ../../../main/cpp/isched/isched_LogEnvLoader.hpp)
target_link_libraries(isched_srv_test PRIVATE isched Catch2::Catch2WithMain spdlog::spdlog httplib::httplib
        nextsilicon-cpp-subprocess::nextsilicon-cpp-subprocess openssl::openssl)
add_test(NAME isched_srv_test COMMAND isched_grammar_tests)

add_executable(basic_server_test ../../basic_server_test.cpp)
target_link_libraries(basic_server_test PRIVATE isched spdlog::spdlog)
add_test(NAME basic_server_test COMMAND basic_server_test)

add_executable(tenant_manager_test ../../tenant_manager_test.cpp)
target_link_libraries(tenant_manager_test PRIVATE isched spdlog::spdlog)
add_test(NAME tenant_manager_test COMMAND tenant_manager_test)

add_executable(rest_hello_world rest_hello_world.cpp
        ../../../main/cpp/isched/isched_LogEnvLoader.hpp)
target_link_libraries(rest_hello_world restbed::restbed)

add_executable(isched_resolver_tests isched_resolver_tests.cpp)
target_link_libraries(isched_resolver_tests isched Catch2::Catch2WithMain)
add_test(NAME isched_resolver_tests COMMAND isched_resolver_tests)

add_executable(database_test database_test.cpp)
target_link_libraries(database_test PRIVATE isched Catch2::Catch2WithMain SQLite::SQLite3 nlohmann_json::nlohmann_json spdlog::spdlog)
add_test(NAME database_test COMMAND database_test)

# New foundational component tests for Universal Application Server Backend
add_executable(isched_auth_tests isched_auth_tests.cpp)
target_link_libraries(isched_auth_tests PRIVATE isched Catch2::Catch2WithMain spdlog::spdlog)
add_test(NAME isched_auth_tests COMMAND isched_auth_tests)

add_executable(isched_ipc_tests isched_ipc_tests.cpp)
target_link_libraries(isched_ipc_tests PRIVATE isched Catch2::Catch2WithMain spdlog::spdlog)
add_test(NAME isched_ipc_tests COMMAND isched_ipc_tests)

add_executable(isched_config_tests isched_config_tests.cpp)
target_link_libraries(isched_config_tests PRIVATE isched Catch2::Catch2WithMain spdlog::spdlog)
add_test(NAME isched_config_tests COMMAND isched_config_tests)

