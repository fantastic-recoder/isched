add_library(isched
        isched.hpp
        GqlParser.cpp
        GqlParser.hpp
)
target_link_libraries(isched PRIVATE taocpp::pegtl spdlog::spdlog)
target_include_directories(isched PRIVATE ${pegtl_INCLUDE_DIRS} PRIVATE ${stdlog_INCLUDE_DIRS})

add_executable(isched_srv isched_srv_main.cpp)
target_link_libraries(isched_srv PRIVATE taocpp::pegtl restbed::restbed spdlog::spdlog)

