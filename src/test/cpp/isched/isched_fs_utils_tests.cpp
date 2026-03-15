// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_fs_utils_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Catch2 tests for `isched_fs_utils` filesystem utilities.
 *
 * Covers `glob()` pattern matching across various wildcard patterns,
 * including recursive `**` patterns and character-class segments.
 */

#include <catch2/catch_all.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <unistd.h>

#include "isched/shared/fs/isched_fs_utils.hpp"

namespace fs = std::filesystem;
using isched::v0_0_1::fsutils::glob;
using isched::v0_0_1::fsutils::read_file;

static void touch(const fs::path &p) {
    fs::create_directories(p.parent_path());
    std::ofstream ofs(p);
    ofs << "x";
}

TEST_CASE("glob: basic patterns and recursive **", "[glob]") {
    // create unique root under temp dir
    auto tdir = fs::temp_directory_path();
    auto pid = static_cast<unsigned long>(::getpid());
    auto now = static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count());
    fs::path tmp_root = tdir / (std::string("isched_glob_test_") + std::to_string(pid) + "_" + std::to_string(now));
    REQUIRE_NOTHROW(fs::create_directories(tmp_root));

    // Layout:
    // root: a.txt, b.md
    // dir1: a.cpp, b.hpp, sub/c.txt
    // dir2: x.txt
    touch(tmp_root / "a.txt");
    touch(tmp_root / "b.md");
    touch(tmp_root / "dir1" / "a.cpp");
    touch(tmp_root / "dir1" / "b.hpp");
    touch(tmp_root / "dir1" / "sub" / "c.txt");
    touch(tmp_root / "dir2" / "x.txt");

    SECTION("*.txt in root") {
        auto res = glob(tmp_root / "*.txt");
        std::vector<fs::path> names;
        for (auto &p : res) names.push_back(p.filename());
        REQUIRE(std::find(names.begin(), names.end(), fs::path("a.txt")) != names.end());
        REQUIRE(std::find(names.begin(), names.end(), fs::path("b.md")) == names.end());
        // Ensure only files at root were matched, not recursive
        for (auto &p : res) {
            REQUIRE(p.parent_path() == fs::weakly_canonical(tmp_root));
        }
    }

    SECTION("**/*.txt recursive") {
        auto res = glob(tmp_root / "**" / "*.txt");
        std::vector<fs::path> names;
        for (auto &p : res) names.push_back(p.filename());
        REQUIRE(std::find(names.begin(), names.end(), fs::path("a.txt")) != names.end());
        REQUIRE(std::find(names.begin(), names.end(), fs::path("c.txt")) != names.end());
        REQUIRE(std::find(names.begin(), names.end(), fs::path("x.txt")) != names.end());
    }

    SECTION("character class [ab].* in dir1") {
        auto res = glob(tmp_root / "dir1" / "[ab].*");
        std::vector<fs::path> names;
        for (auto &p : res) names.push_back(p.filename());
        REQUIRE(std::find(names.begin(), names.end(), fs::path("a.cpp")) != names.end());
        REQUIRE(std::find(names.begin(), names.end(), fs::path("b.hpp")) != names.end());
    }

    SECTION("single char ? matches one") {
        auto res = glob(tmp_root / "dir1" / "?.hpp");
        REQUIRE(res.size() == 1);
        REQUIRE(res[0].filename() == fs::path("b.hpp"));
    }

    SECTION("**/sub/*.txt specific subdir") {
        auto res = glob(tmp_root / "**" / "sub" / "*.txt");
        REQUIRE(res.size() == 1);
        REQUIRE(res[0].filename() == fs::path("c.txt"));
    }

    SECTION("parent directory .. in pattern") {
        // pattern: tmp_root/dir1/../dir2/*.txt -> should match tmp_root/dir2/x.txt
        auto res = glob(tmp_root / "dir1" / ".." / "dir2" / "*.txt");
        REQUIRE(res.size() == 1);
        REQUIRE(res[0].filename() == fs::path("x.txt"));
    }

    SECTION("current directory . in pattern") {
        // pattern: tmp_root/dir1/./a.cpp -> should match tmp_root/dir1/a.cpp
        auto res = glob(tmp_root / "dir1" / "." / "a.cpp");
        REQUIRE(res.size() == 1);
        REQUIRE(res[0].filename() == fs::path("a.cpp"));
    }

    SECTION("relative path with ..") {
        auto old_pwd = fs::current_path();
        fs::current_path(tmp_root / "dir1");
        auto res = glob(fs::path("..") / "dir2" / "*.txt");
        fs::current_path(old_pwd);
        REQUIRE(res.size() == 1);
        REQUIRE(res[0].filename() == fs::path("x.txt"));
    }

    // Cleanup
    REQUIRE_NOTHROW(fs::remove_all(tmp_root));
}

TEST_CASE("read_file: basic functionality", "[fsutils]") {
    auto tdir = fs::temp_directory_path();
    auto pid = static_cast<unsigned long>(::getpid());
    auto now = static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count());
    fs::path const tmp_root = tdir / (std::string("isched_read_file_test_") + std::to_string(pid) + "_" + std::to_string(now));
    REQUIRE_NOTHROW(fs::create_directories(tmp_root));

    SECTION("read normal text file") {
        fs::path const p = tmp_root / "test.txt";
        std::string content = "Hello, World!\nLine 2";
        {
            std::ofstream ofs(p);
            ofs << content;
        }
        REQUIRE(read_file(p) == content);
    }

    SECTION("read empty file") {
        fs::path const p = tmp_root / "empty.txt";
        {
            std::ofstream const ofs(p);
        }
        REQUIRE(read_file(p).empty());
    }

    SECTION("read binary-like data") {
        fs::path const p = tmp_root / "binary.dat";
        std::string content = "A\0B\n\r\tC";
        content[1] = '\0'; // ensure null byte
        {
            std::ofstream ofs(p, std::ios::binary);
            ofs.write(content.data(), content.size());
        }
        std::string read = read_file(p);
        REQUIRE(read.size() == content.size());
        REQUIRE(read == content);
    }

    SECTION("throw on non-existent file") {
        fs::path const p = tmp_root / "does_not_exist.txt";
        REQUIRE_THROWS_AS(read_file(p), std::runtime_error);
    }

    // Cleanup
    REQUIRE_NOTHROW(fs::remove_all(tmp_root));
}
