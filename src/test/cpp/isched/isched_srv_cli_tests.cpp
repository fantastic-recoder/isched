// SPDX-License-Identifier: MPL-2.0

#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>

#include <catch2/catch_test_macros.hpp>

#include "isched/backend/isched_AuthenticationMiddleware.hpp"
#include "isched/backend/isched_DatabaseManager.hpp"
#include "isched/backend/isched_srv_cli.hpp"

using namespace isched::v0_0_1::backend;
using namespace isched::v0_0_1::backend::srvcli;

TEST_CASE("srv CLI parser accepts valid --seed option", "[srv][cli][seed]") {
    const char* argv[] = {"isched_srv", "--seed", "admin:supersecret", "--data-dir", "/tmp/isched"};
    std::string error;

    const auto parsed = parse_cli_args(static_cast<int>(std::size(argv)), const_cast<char**>(argv), error);

    REQUIRE(parsed);
    REQUIRE(parsed->seed_credentials);
    CHECK(parsed->seed_credentials->admin_name == "admin");
    CHECK(parsed->seed_credentials->password == "supersecret");
    REQUIRE(parsed->data_dir);
    CHECK(*parsed->data_dir == "/tmp/isched");
}

TEST_CASE("srv CLI parser accepts equals-form options", "[srv][cli][seed]") {
    const char* argv[] = {"isched_srv", "--seed=admin:supersecret", "--data-dir=/tmp/isched", "--help"};
    std::string error;

    const auto parsed = parse_cli_args(static_cast<int>(std::size(argv)), const_cast<char**>(argv), error);

    REQUIRE(parsed);
    REQUIRE(parsed->seed_credentials);
    CHECK(parsed->seed_credentials->admin_name == "admin");
    CHECK(parsed->seed_credentials->password == "supersecret");
    REQUIRE(parsed->data_dir);
    CHECK(*parsed->data_dir == "/tmp/isched");
    CHECK(parsed->help_requested);
}

TEST_CASE("srv CLI parser rejects malformed --seed option", "[srv][cli][seed]") {
    SECTION("missing colon") {
        const char* argv[] = {"isched_srv", "--seed", "adminonly"};
        std::string error;
        const auto parsed = parse_cli_args(static_cast<int>(std::size(argv)), const_cast<char**>(argv), error);
        REQUIRE_FALSE(parsed);
        CHECK(error.find("must contain ':'") != std::string::npos);
    }

    SECTION("empty admin") {
        const char* argv[] = {"isched_srv", "--seed", ":password"};
        std::string error;
        const auto parsed = parse_cli_args(static_cast<int>(std::size(argv)), const_cast<char**>(argv), error);
        REQUIRE_FALSE(parsed);
        CHECK(error.find("admin name") != std::string::npos);
    }

    SECTION("empty password") {
        const char* argv[] = {"isched_srv", "--seed", "admin:"};
        std::string error;
        const auto parsed = parse_cli_args(static_cast<int>(std::size(argv)), const_cast<char**>(argv), error);
        REQUIRE_FALSE(parsed);
        CHECK(error.find("password") != std::string::npos);
    }

    SECTION("missing option value") {
        const char* argv[] = {"isched_srv", "--seed"};
        std::string error;
        const auto parsed = parse_cli_args(static_cast<int>(std::size(argv)), const_cast<char**>(argv), error);
        REQUIRE_FALSE(parsed);
        CHECK(error.find("requires a value") != std::string::npos);
    }

    SECTION("missing option value in equals form") {
        const char* argv[] = {"isched_srv", "--seed="};
        std::string error;
        const auto parsed = parse_cli_args(static_cast<int>(std::size(argv)), const_cast<char**>(argv), error);
        REQUIRE_FALSE(parsed);
        CHECK(error.find("non-empty value") != std::string::npos);
    }
}

TEST_CASE("srv CLI parser rejects malformed --data-dir option", "[srv][cli][data-dir]") {
    SECTION("missing value") {
        const char* argv[] = {"isched_srv", "--data-dir"};
        std::string error;
        const auto parsed = parse_cli_args(static_cast<int>(std::size(argv)), const_cast<char**>(argv), error);
        REQUIRE_FALSE(parsed);
        CHECK(error.find("--data-dir requires a value") != std::string::npos);
    }

    SECTION("empty equals value") {
        const char* argv[] = {"isched_srv", "--data-dir="};
        std::string error;
        const auto parsed = parse_cli_args(static_cast<int>(std::size(argv)), const_cast<char**>(argv), error);
        REQUIRE_FALSE(parsed);
        CHECK(error.find("non-empty value") != std::string::npos);
    }

    SECTION("unknown option") {
        const char* argv[] = {"isched_srv", "--wat"};
        std::string error;
        const auto parsed = parse_cli_args(static_cast<int>(std::size(argv)), const_cast<char**>(argv), error);
        REQUIRE_FALSE(parsed);
        CHECK(error.find("--wat") != std::string::npos);
    }
}

TEST_CASE("seed confirmation requires exact yes", "[srv][cli][seed]") {
    SECTION("accept exact yes") {
        std::istringstream in{"yes\n"};
        std::ostringstream out;
        CHECK(prompt_seed_confirmation(in, out));
        CHECK(out.str() == "really wipe out platform DB and reset admin password?\n");
    }

    SECTION("reject non-confirmation response") {
        std::istringstream in{"Yes\n"};
        std::ostringstream out;
        CHECK_FALSE(prompt_seed_confirmation(in, out));
    }
}

TEST_CASE("seed flow backs up existing DB and creates seeded platform admin", "[srv][cli][seed]") {
    namespace fs = std::filesystem;

    const auto unique = "isched_srv_cli_seed_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto work_dir = fs::temp_directory_path() / unique;
    REQUIRE(fs::create_directories(work_dir));

    const auto db_path = work_dir / "isched_system.db";
    {
        std::ofstream old_db{db_path};
        REQUIRE(old_db.good());
        old_db << "legacy platform db";
    }

    const SeedCredentials creds{"seed_admin", "seed_password_12345"};
    std::string error;
    const auto result = reset_and_seed_platform_db(work_dir.string(), creds, error);

    REQUIRE(result);
    REQUIRE(result->backup_path);
    REQUIRE(fs::exists(*result->backup_path));

    std::ifstream backup_file{*result->backup_path};
    std::string backup_payload;
    std::getline(backup_file, backup_payload);
    CHECK(backup_payload == "legacy platform db");

    DatabaseManager::Config cfg;
    cfg.base_path = (work_dir / "tenants").string();
    cfg.system_db_path = db_path.string();
    auto db = std::make_shared<DatabaseManager>(cfg);
    REQUIRE(db->ensure_system_db());

    auto admin = db->get_platform_admin_by_email("seed_admin");
    REQUIRE(admin);
    CHECK(admin.value().email == "seed_admin");
    CHECK(verify_password("seed_password_12345", admin.value().password_hash));

    std::error_code ec;
    fs::remove_all(work_dir, ec);
}



