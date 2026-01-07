#include <catch2/catch_test_macros.hpp>
#include <isched/backend/isched_multi_dim_map.hpp>
#include <string>

using namespace isched::v0_0_1::backend;

TEST_CASE("multi_dim_map basic functionality", "[multi_dim_map]") {
    multi_dim_map<std::string, int> my_mmap;
    
    SECTION("Initial requirement test") {
        my_mmap[0] = 0;
        my_mmap[1] = 1;
        my_mmap[0][0] = 2;
        my_mmap[0][0] = 3;
        REQUIRE(my_mmap[0][0] == 3);
        REQUIRE(my_mmap[0] == 0);
        REQUIRE(my_mmap[1] == 1);
    }

    SECTION("String keys") {
        my_mmap["first"] = 10;
        my_mmap["first"]["second"] = 20;
        REQUIRE(my_mmap["first"] == 10);
        REQUIRE(my_mmap["first"]["second"] == 20);
    }
    
    SECTION("Deep nesting") {
        my_mmap["a"]["b"]["c"]["d"] = 42;
        REQUIRE(my_mmap["a"]["b"]["c"]["d"] == 42);
    }
}

TEST_CASE("multi_dim_map path functionality", "[multi_dim_map]") {
    multi_dim_map<std::string, int> my_mmap;
    SECTION("Deep nesting path") {
        my_mmap["a"]["b"]["c"]["d"] = 42;
        my_mmap["a"]["b"]=21;
        std::vector<multi_dim_map<std::string, int>::key_type> path = {"a", "b", "c", "d"};
        REQUIRE(my_mmap[path] == 42);
        REQUIRE(my_mmap.at(path) == 42);
        std::vector<multi_dim_map<std::string, int>::key_type> path2 = {"a", "b"};
        REQUIRE(my_mmap.at(path2) == 21);
    }
}

TEST_CASE("multi_dim_map STL container methods", "[multi_dim_map]") {
    multi_dim_map<std::string, int> my_mmap;
    my_mmap["a"] = 1;
    my_mmap["b"] = 2;
    my_mmap["c"] = 3;

    SECTION("Typedefs") {
        STATIC_REQUIRE(std::is_same_v<multi_dim_map<std::string, int>::key_type, std::string>);
        STATIC_REQUIRE(std::is_same_v<multi_dim_map<std::string, int>::mapped_type, multi_dim_map<std::string, int>>);
        STATIC_REQUIRE(std::is_same_v<multi_dim_map<std::string, int>::value_type, std::pair<const std::string, multi_dim_map<std::string, int>>>);
        STATIC_REQUIRE(std::is_same_v<multi_dim_map<std::string, int>::size_type, std::size_t>);
    }

    SECTION("Iteration") {
        int count = 0;
        for (auto it = my_mmap.begin(); it != my_mmap.end(); ++it) {
            count++;
            if (it->first == "a") REQUIRE(it->second == 1);
            if (it->first == "b") REQUIRE(it->second == 2);
            if (it->first == "c") REQUIRE(it->second == 3);
        }
        REQUIRE(count == 3);

        // Range-based for loop
        count = 0;
        for (const auto& [key, val] : my_mmap) {
            count++;
            if (key == "a") REQUIRE(val == 1);
        }
        REQUIRE(count == 3);
    }

    SECTION("Capacity and Lookup") {
        REQUIRE(my_mmap.size() == 3);
        REQUIRE_FALSE(my_mmap.empty());
        REQUIRE(my_mmap.count("a") == 1);
        REQUIRE(my_mmap.count("d") == 0);
        REQUIRE(my_mmap.find("b") != my_mmap.end());
        REQUIRE(my_mmap.find("e") == my_mmap.end());
        REQUIRE(my_mmap.contains("c"));
    }

    SECTION("Modifiers") {
        my_mmap.erase("b");
        REQUIRE(my_mmap.size() == 2);
        REQUIRE(my_mmap.find("b") == my_mmap.end());

        my_mmap.clear();
        REQUIRE(my_mmap.size() == 0);
        REQUIRE(my_mmap.empty());
    }
}