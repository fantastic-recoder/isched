add_library(isched
        isched.hpp
        shared/isched_exception_base.cpp
        shared/isched_exception_base.hpp
        shared/isched_exception_doc_path_not_found.cpp
        shared/isched_exception_doc_path_not_found.hpp
        shared/isched_exception_unknown_enum_value.hpp
        backend/isched_gql_parser.cpp
        backend/isched_gql_parser.hpp
        backend/isched_doc_root_rest_resolver.cpp
        backend/isched_doc_root_rest_resolver.hpp
        backend/isched_main_svc.cpp
        backend/isched_main_svc.hpp
        backend/isched_single_action_resolver.cpp
        backend/isched_single_action_resolver.hpp
        backend/isched_BaseRestResolver.cpp
        backend/isched_BaseRestResolver.hpp
        backend/isched_e_http_methods.hpp
        backend/isched_log_env_loader.hpp
        # Universal Application Server Backend components
        backend/isched_common.hpp
        backend/isched_server.hpp
        backend/isched_server.cpp
        backend/isched_tenant_manager.hpp
        backend/isched_tenant_manager.cpp
        backend/isched_database.hpp
        backend/isched_database.cpp
        backend/isched_graphql_executor.hpp
        backend/isched_graphql_executor.cpp
        backend/isched_auth.hpp
        backend/isched_auth.cpp
        backend/isched_built_in_schema.hpp
        backend/isched_built_in_schema.cpp
        shared/ipc/isched_ipc.hpp
        shared/ipc/isched_ipc.cpp
        shared/config/isched_config.hpp
        shared/config/isched_config.cpp
        shared/fs/isched_glob.hpp
        shared/fs/isched_glob.cpp
        backend/isched_gql_grammar.hpp
)
target_link_libraries(isched PRIVATE taocpp::pegtl restbed::restbed spdlog::spdlog nlohmann_json::nlohmann_json jwt-cpp::jwt-cpp SQLite::SQLite3 Boost::url)
target_include_directories(isched PRIVATE ${pegtl_INCLUDE_DIRS} PRIVATE ${stdlog_INCLUDE_DIRS})

add_executable(isched_srv backend/isched_srv_main.cpp)
target_link_libraries(isched_srv PRIVATE isched spdlog::spdlog openssl::openssl)
target_include_directories(isched_srv PRIVATE ${openssl_INCLUDE_DIRS})

