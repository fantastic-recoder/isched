add_library(isched
        isched.hpp
        GqlParser.cpp
        GqlParser.hpp
)
target_link_libraries(isched PRIVATE taocpp::pegtl spdlog::spdlog)

add_executable(isched_srv isched_srv_main.cpp)
target_link_libraries(isched_srv PRIVATE taocpp::pegtl)

