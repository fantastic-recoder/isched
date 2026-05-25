// SPDX-License-Identifier: MPL-2.0

#include "isched_srv_cli.hpp"

#include <chrono>
#include <exception>
#include <filesystem>
#include <functional>
#include <memory>

#include <spdlog/spdlog.h>

#include "isched_AuthenticationMiddleware.hpp"
#include "isched_DatabaseManager.hpp"

namespace isched::v0_0_1::backend::srvcli {

namespace {

std::string make_backup_suffix() {
    const auto now = std::chrono::system_clock::now();
    const auto ts = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    return ".bak." + std::to_string(ts);
}

bool backup_if_present(const std::filesystem::path& db_path,
                       std::optional<std::filesystem::path>& backup_path,
                       std::string& error) {
    std::error_code ec;
    if (!std::filesystem::exists(db_path, ec)) {
        if (ec) {
            error = "failed to check platform DB path '" + db_path.string() + "': " + ec.message();
            return false;
        }
        return true;
    }

    if (!std::filesystem::is_regular_file(db_path, ec) || ec) {
        error = "platform DB path exists but is not a regular file: '" + db_path.string() + "'";
        return false;
    }

    const auto candidate = std::filesystem::path(db_path.string() + make_backup_suffix());
    std::filesystem::copy_file(db_path, candidate, std::filesystem::copy_options::none, ec);
    if (ec) {
        error = "failed to back up platform DB to '" + candidate.string() + "': " + ec.message();
        return false;
    }

    backup_path = candidate;
    return true;
}

bool remove_if_present(const std::filesystem::path& path, std::string& error) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        if (ec) {
            error = "failed to check path '" + path.string() + "': " + ec.message();
            return false;
        }
        return true;
    }

    std::filesystem::remove(path, ec);
    if (ec) {
        error = "failed to remove '" + path.string() + "': " + ec.message();
        return false;
    }

    return true;
}

} // namespace

std::optional<SeedCredentials> parse_seed_credentials(std::string_view value,
                                                      std::string& error) {
    if (value.empty()) {
        error = "--seed requires a non-empty value in format <admin_name>:<password>";
        return std::nullopt;
    }

    const auto colon = value.find(':');
    if (colon == std::string_view::npos) {
        error = "--seed value must contain ':' separator in format <admin_name>:<password>";
        return std::nullopt;
    }

    const std::string admin_name{value.substr(0, colon)};
    const std::string password{value.substr(colon + 1)};

    if (admin_name.empty()) {
        error = "--seed admin name must not be empty";
        return std::nullopt;
    }

    if (password.empty()) {
        error = "--seed password must not be empty";
        return std::nullopt;
    }

    return SeedCredentials{admin_name, password};
}

std::optional<CliOptions> parse_cli_args(int argc, char** argv,
                                         std::string& error) {
    CliOptions opts;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};

        if (arg == "--help" || arg == "-h") {
            opts.help_requested = true;
            continue;
        }

        if (arg == "--data-dir") {
            if (i + 1 >= argc) {
                error = "--data-dir requires a value";
                return std::nullopt;
            }
            opts.data_dir = argv[++i];
            continue;
        }

        if (arg.starts_with("--data-dir=")) {
            const std::string value{arg.substr(std::string_view{"--data-dir="}.size())};
            if (value.empty()) {
                error = "--data-dir requires a non-empty value";
                return std::nullopt;
            }
            opts.data_dir = value;
            continue;
        }

        if (arg == "--seed") {
            if (i + 1 >= argc) {
                error = "--seed requires a value in format <admin_name>:<password>";
                return std::nullopt;
            }
            const auto parsed = parse_seed_credentials(argv[++i], error);
            if (!parsed) {
                return std::nullopt;
            }
            opts.seed_credentials = *parsed;
            continue;
        }

        if (arg.starts_with("--seed=")) {
            const auto parsed = parse_seed_credentials(
                arg.substr(std::string_view{"--seed="}.size()), error);
            if (!parsed) {
                return std::nullopt;
            }
            opts.seed_credentials = *parsed;
            continue;
        }

        error = "Unknown option: " + std::string(arg);
        return std::nullopt;
    }

    return opts;
}

bool prompt_seed_confirmation(std::istream& in, std::ostream& out) {
    out << "really wipe out platform DB and reset admin password?\n";
    out.flush();

    std::string answer;
    if (!std::getline(in, answer)) {
        return false;
    }

    return answer == "yes";
}

std::optional<SeedResult> reset_and_seed_platform_db(const std::string& work_directory,
                                                     const SeedCredentials& credentials,
                                                     std::string& error) {
    if (work_directory.empty()) {
        error = "work_directory must not be empty";
        return std::nullopt;
    }

    std::error_code ec;
    std::filesystem::create_directories(work_directory, ec);
    if (ec) {
        error = "failed to create work directory '" + work_directory + "': " + ec.message();
        return std::nullopt;
    }

    const auto system_db_path = std::filesystem::path(work_directory) / "isched_system.db";
    SeedResult result{};

    if (!backup_if_present(system_db_path, result.backup_path, error)) {
        return std::nullopt;
    }

    if (!remove_if_present(system_db_path, error) ||
        !remove_if_present(system_db_path.string() + "-wal", error) ||
        !remove_if_present(system_db_path.string() + "-shm", error)) {
        return std::nullopt;
    }

    DatabaseManager::Config db_config;
    db_config.base_path = work_directory + "/tenants";
    db_config.system_db_path = system_db_path.string();

    auto db = std::make_shared<DatabaseManager>(db_config);
    if (auto ensure_res = db->ensure_system_db(); !ensure_res) {
        error = "failed to initialize platform DB";
        return std::nullopt;
    }

    try {
        const auto password_hash = hash_password(credentials.password);
        const auto admin_id = "pa_" + std::to_string(std::hash<std::string>{}(credentials.admin_name));

        if (auto create_res = db->create_platform_admin(
                admin_id,
                credentials.admin_name,
                password_hash,
                credentials.admin_name);
            !create_res)
        {
            error = "failed to create seeded platform admin account";
            return std::nullopt;
        }
    } catch (const std::exception& ex) {
        error = "failed to hash/store seeded admin credentials: " + std::string(ex.what());
        return std::nullopt;
    }

    if (result.backup_path) {
        spdlog::info("Seed reset: backup written to {}", result.backup_path->string());
    }
    return result;
}

} // namespace isched::v0_0_1::backend::srvcli



