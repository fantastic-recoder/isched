add_executable(isched_grammar_tests isched_grammar_tests.cpp)
target_link_libraries(isched_grammar_tests PRIVATE taocpp::pegtl restbed::restbed spdlog::spdlog isched Catch2::Catch2WithMain)
add_test(NAME isched_grammar_tests COMMAND isched_grammar_tests)

add_executable(isched_srv_test isched_srv_tests.cpp)
target_link_libraries(isched_srv_test isched Catch2::Catch2WithMain)
add_test(NAME isched_srv_test COMMAND isched_grammar_tests)

add_executable(rest_hello_world rest_hello_world.cpp)
target_link_libraries(rest_hello_world restbed::restbed)

