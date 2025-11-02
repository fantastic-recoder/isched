add_library(isched
        isched.hpp
        isched_GqlParser.cpp
        isched_GqlParser.hpp
        isched_DocRootResolver.cpp
        isched_DocRootResolver.hpp
        isched_MainSvc.cpp
        isched_MainSvc.hpp
        isched_ExceptionBase.cpp
        isched_ExceptionBase.hpp
        isched_ExceptionDocPathNotFound.cpp
        isched_ExceptionDocPathNotFound.hpp
        isched_SingleActionResolver.cpp
        isched_SingleActionResolver.hpp
        isched_BaseResolver.cpp
        isched_BaseResolver.hpp
        isched_EHttpMethods.hpp
        isched_ExceptionUnknownEnumValue.hpp
        isched_LogEnvLoader.hpp
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
)
target_link_libraries(isched PRIVATE taocpp::pegtl restbed::restbed spdlog::spdlog nlohmann_json::nlohmann_json jwt-cpp::jwt-cpp SQLite::SQLite3)
target_include_directories(isched PRIVATE ${pegtl_INCLUDE_DIRS} PRIVATE ${stdlog_INCLUDE_DIRS})

add_executable(isched_srv isched_srv_main.cpp)
target_link_libraries(isched_srv PRIVATE isched spdlog::spdlog openssl::openssl)
target_include_directories(isched_srv PRIVATE ${openssl_INCLUDE_DIRS})

