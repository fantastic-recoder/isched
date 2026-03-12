// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_multi_dim_map_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Catch2 tests for the `multi_dim_map` container.
 *
 * Exercises basic get/set operations, multi-level nesting, and edge cases
 * of `isched::v0_0_1::backend::multi_dim_map`.
 */

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
        my_mmap[std::vector<std::string>{}]["a"]=11;
        REQUIRE(my_mmap["a"] == 11);
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
        
        auto it = my_mmap.find("b");
        REQUIRE(it != my_mmap.end());
        REQUIRE(it->first == "b");
        REQUIRE(it->second.has_value());
        REQUIRE(it->second.get_value() == 2);

        REQUIRE(my_mmap.find("e") == my_mmap.end());
        REQUIRE(my_mmap.contains("c"));
    }

    SECTION("Find and value retrieval") {
        my_mmap["deep"]["node"] = 42;
        
        auto it = my_mmap.find("deep");
        REQUIRE(it != my_mmap.end());
        REQUIRE(it->first == "deep");
        REQUIRE_FALSE(it->second.has_value()); // "deep" doesn't have a value itself
        
        auto it_inner = it->second.find("node");
        REQUIRE(it_inner != it->second.end());
        REQUIRE(it_inner->first == "node");
        REQUIRE(it_inner->second.has_value());
        REQUIRE(it_inner->second.get_value() == 42);
        
        // Retrieval via conversion
        int val = it_inner->second;
        REQUIRE(val == 42);
    }

    SECTION("Find should search only in subnet") {
        my_mmap["x"]["x0"] = 100;
        my_mmap["x"]["x1"] = 101;
        my_mmap["y"]["y0"] = 102;
        my_mmap["y"]["y1"] = 103;
        auto it0 = my_mmap[std::vector<std::string>{"x"}].find("y0");
        REQUIRE(it0 == my_mmap[std::vector<std::string>{"x"}].end());
        it0 = my_mmap[std::vector<std::string>{"x"}].find("y");
        REQUIRE(it0 == my_mmap[std::vector<std::string>{"x"}].end());
    }

    SECTION("Modifiers") {
        my_mmap.erase("b");
        REQUIRE(my_mmap.size() == 2);
        REQUIRE(my_mmap.find("b") == my_mmap.end());

        my_mmap.clear();
        REQUIRE(my_mmap.size() == 0);
        REQUIRE(my_mmap.empty());
    }

    SECTION("Emplace") {
        my_mmap.emplace("d", 4);
        REQUIRE(my_mmap.size() == 4);
        REQUIRE(my_mmap.contains("d"));
        REQUIRE(my_mmap["d"] == 4);

        // Nested emplace
        my_mmap["d"].emplace("e", 5);
        REQUIRE(my_mmap["d"]["e"] == 5);
    }
}

TEST_CASE("multi_dim_map basic int functionality", "[multi_dim_map]") {
    multi_dim_map<int, int> my_mmap;

    SECTION("Initial requirement test") {
        my_mmap[0] = 0;
        my_mmap[1] = 1;
        my_mmap[0][0] = 2;
        my_mmap[0][0] = 3;
        REQUIRE(my_mmap[0][0] == 3);
        REQUIRE(my_mmap[0] == 0);
        REQUIRE(my_mmap[1] == 1);
    }
}

