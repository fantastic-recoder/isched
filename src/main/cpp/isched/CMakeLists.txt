add_library(isched
        isched.hpp
        isched_gql_parser.cpp
        isched_gql_parser.hpp
        isched_doc_root_resolver.cpp
        isched_doc_root_resolver.hpp
        isched_main_svc.cpp
        isched_main_svc.hpp
        isched_exception_base.cpp
        isched_exception_base.hpp
        isched_exception_doc_path_not_found.cpp
        isched_exception_doc_path_not_found.hpp
        isched_single_action_resolver.cpp
        isched_single_action_resolver.hpp
        isched_base_resolver.cpp
        isched_base_resolver.hpp
        isched_e_http_methods.hpp
        isched_exception_unknown_enum_value.hpp
        isched_log_env_loader.hpp
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
        isched_gql_grammar.hpp
)
target_link_libraries(isched PRIVATE taocpp::pegtl restbed::restbed spdlog::spdlog nlohmann_json::nlohmann_json jwt-cpp::jwt-cpp SQLite::SQLite3 Boost::url)
target_include_directories(isched PRIVATE ${pegtl_INCLUDE_DIRS} PRIVATE ${stdlog_INCLUDE_DIRS})

add_executable(isched_srv isched_srv_main.cpp)
target_link_libraries(isched_srv PRIVATE isched spdlog::spdlog openssl::openssl)
target_include_directories(isched_srv PRIVATE ${openssl_INCLUDE_DIRS})

