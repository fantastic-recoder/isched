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
)
target_link_libraries(isched PRIVATE taocpp::pegtl restbed::restbed spdlog::spdlog)
target_include_directories(isched PRIVATE ${pegtl_INCLUDE_DIRS} PRIVATE ${stdlog_INCLUDE_DIRS})

add_executable(isched_srv isched_srv_main.cpp)
target_link_libraries(isched_srv PRIVATE isched spdlog::spdlog)

