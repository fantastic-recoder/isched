#include <catch2/catch_all.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <unistd.h>

#include "../../../main/cpp/isched/shared/fs/isched_glob.hpp"

namespace fs = std::filesystem;
using isched::v0_0_1::fsutils::glob;

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

    // Cleanup
    REQUIRE_NOTHROW(fs::remove_all(tmp_root));
}
