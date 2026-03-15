# Reads the file and escapes it for a C string
file(READ "backend/isched_builtin_server_schema.graphql" SCHEMA_CONTENT)

# Create a generated header
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/isched_builtin_server_schema.hpp"
        "namespace { const char* BUILTIN_SCHEMA = R\"graphql(${SCHEMA_CONTENT})graphql\";}")

add_library(isched
        isched.hpp
        shared/isched_exception_base.cpp
        shared/isched_exception_base.hpp
        shared/isched_exception_doc_path_not_found.cpp
        shared/isched_exception_doc_path_not_found.hpp
        shared/isched_exception_unknown_enum_value.hpp
        backend/isched_LogEnvLoader.hpp
        # Universal Application Server Backend components
        backend/isched_common.hpp
        backend/isched_Server.hpp
        backend/isched_Server.cpp
        backend/isched_TenantManager.hpp
        backend/isched_TenantManager.cpp
        backend/isched_DatabaseManager.hpp
        backend/isched_DatabaseManager.cpp
        backend/isched_AuthenticationMiddleware.hpp
        backend/isched_AuthenticationMiddleware.cpp
        backend/isched_gql_grammar.cpp
        shared/config/isched_config.hpp
        shared/config/isched_config.cpp
        shared/fs/isched_fs_utils.hpp
        shared/fs/isched_fs_utils.cpp
        backend/isched_gql_grammar.hpp
        backend/isched_GqlExecutor.cpp
        backend/isched_GqlExecutor.hpp
        backend/isched_ExecutionResult.hpp
        backend/isched_SubscriptionBroker.hpp
        backend/isched_SubscriptionBroker.cpp
        backend/isched_gql_error.hpp
        backend/isched_multi_dim_map.hpp
        backend/isched_gql_grammar.cpp
        backend/isched_CryptoUtils.hpp
        backend/isched_CryptoUtils.cpp
        backend/isched_RestDataSource.hpp
        backend/isched_RestDataSource.cpp
)
target_link_libraries(isched PRIVATE taocpp::pegtl httplib::httplib openssl::openssl spdlog::spdlog nlohmann_json::nlohmann_json
        jwt-cpp::jwt-cpp SQLite::SQLite3 Boost::url Boost::system Boost::thread sago::platform_folders)
target_include_directories(isched PRIVATE ${pegtl_INCLUDE_DIRS} PRIVATE ${stdlog_INCLUDE_DIRS} "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(isched_srv backend/isched_srv_main.cpp)
target_link_libraries(isched_srv PRIVATE isched spdlog::spdlog Boost::url openssl::openssl)
target_include_directories(isched_srv PRIVATE ${openssl_INCLUDE_DIRS})


