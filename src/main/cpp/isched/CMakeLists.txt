# Reconfigure when the embedded GraphQL schema changes.
include(GNUInstallDirs)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/backend/isched_builtin_server_schema.graphql")
# Reads the file and embeds it in a C string.
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
        shared/ast/isched_NodeType.hpp
        shared/ast/isched_CustomNode.hpp
        shared/ast/isched_CustomNode.cpp
        shared/ast/isched_NodeSelector.hpp
        shared/ast/isched_NonRecursiveVisitor.hpp
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
        backend/isched_GqlExecutor_field_nodes.cpp
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
        backend/isched_MetricsCollector.hpp
        backend/isched_MetricsCollector.cpp
        backend/isched_UiAssetRegistry.hpp
        backend/isched_UiAssetRegistry.cpp
        backend/isched_RateLimiter.hpp
        backend/isched_PluginSystem.hpp
        backend/isched_PluginSystem.cpp
        backend/isched_srv_cli.hpp
        backend/isched_srv_cli.cpp
)
target_link_libraries(isched
        PUBLIC Boost::url
        PRIVATE taocpp::pegtl httplib::httplib openssl::openssl spdlog::spdlog nlohmann_json::nlohmann_json
                jwt-cpp::jwt-cpp SQLite::SQLite3 Boost::system Boost::thread Boost::program_options
                sago::platform_folders Boost::filesystem ${CMAKE_DL_LIBS})
target_include_directories(isched
        PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/main/cpp>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE ${pegtl_INCLUDE_DIRS} ${stdlog_INCLUDE_DIRS} "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(isched PUBLIC cxx_std_23)

add_executable(isched_srv backend/isched_srv_main.cpp)
target_link_libraries(isched_srv PRIVATE isched spdlog::spdlog Boost::url openssl::openssl)
target_include_directories(isched_srv PRIVATE ${openssl_INCLUDE_DIRS})

if(ISCHED_ENABLE_PACKAGING)
    include("${PROJECT_SOURCE_DIR}/packaging/sdk/public-headers.cmake")

    set_target_properties(isched PROPERTIES EXPORT_NAME isched)
    install(TARGETS isched
            EXPORT ischedTargets
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT devel)
    install(TARGETS isched_srv
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT runtime)
    install(EXPORT ischedTargets
            FILE ischedTargets.cmake
            NAMESPACE isched::
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/isched"
            COMPONENT devel)

    foreach(header IN LISTS ISCHED_PUBLIC_HEADER_RELATIVE_PATHS)
        get_filename_component(header_directory "${header}" DIRECTORY)
        if(header_directory STREQUAL ".")
            set(header_destination "${CMAKE_INSTALL_INCLUDEDIR}/isched")
        else()
            set(header_destination "${CMAKE_INSTALL_INCLUDEDIR}/isched/${header_directory}")
        endif()
        install(FILES "${PROJECT_SOURCE_DIR}/src/main/cpp/isched/${header}"
                DESTINATION "${header_destination}" COMPONENT devel)
    endforeach()
endif()
