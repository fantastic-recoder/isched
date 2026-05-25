// SPDX-License-Identifier: MPL-2.0

#include "isched_srv_cli.hpp"

#include <boost/program_options.hpp>

#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <memory>
#include <cctype>

#include <spdlog/spdlog.h>

#include "isched_AuthenticationMiddleware.hpp"
#include "isched_DatabaseManager.hpp"

namespace isched::v0_0_1::backend::srvcli {

namespace po = boost::program_options;

namespace {

bool is_valid_email_address(std::string_view email) {
    if (email.empty() || email.find(' ') != std::string_view::npos) {
        return false;
    }

    const auto at = email.find('@');
    if (at == std::string_view::npos || at == 0 || at == email.size() - 1) {
        return false;
    }
    if (email.find('@', at + 1) != std::string_view::npos) {
        return false;
    }

    const auto local = email.substr(0, at);
    const auto domain = email.substr(at + 1);
    if (local.empty() || domain.empty()) {
        return false;
    }

    if (local.front() == '.' || local.back() == '.') {
        return false;
    }

    for (const char c : local) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (!std::isalnum(uc) && c != '.' && c != '_' && c != '%' && c != '+' && c != '-') {
            return false;
        }
    }

    if (domain.find('.') == std::string_view::npos || domain.front() == '.' || domain.back() == '.') {
        return false;
    }

    std::size_t label_start = 0;
    while (label_start < domain.size()) {
        const auto dot = domain.find('.', label_start);
        const auto label_end = dot == std::string_view::npos ? domain.size() : dot;
        const auto label_len = label_end - label_start;
        if (label_len == 0) {
            return false;
        }

        if (domain[label_start] == '-' || domain[label_end - 1] == '-') {
            return false;
        }

        for (std::size_t i = label_start; i < label_end; ++i) {
            const unsigned char uc = static_cast<unsigned char>(domain[i]);
            if (!std::isalnum(uc) && domain[i] != '-') {
                return false;
            }
        }

        if (dot == std::string_view::npos) {
            break;
        }
        label_start = dot + 1;
    }

    return true;
}

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

std::string build_usage_text(std::string_view program) {
    std::ostringstream out;
    out << "Usage: " << program << " [--data-dir <path>] [--seed <admin_name>:<password>] [--help]\n"
        << "\n"
        << "Options:\n"
        << "  --data-dir <path>   Override server work directory (tenant data under <path>/tenants)\n"
        << "  --data-dir=<path>   Same as above\n"
        << "  --seed <a>:<p>      Confirmed platform DB reset + bootstrap admin credential seeding\n"
        << "  --seed=<a>:<p>      Same as above\n"
        << "  --help              Show this help text\n";
    return out.str();
}

bool validate_data_dir(const std::string& path, std::string& error) {
    if (path.empty()) {
        error = "data directory must not be empty";
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) {
        error = "failed to create data directory '" + path + "': " + ec.message();
        return false;
    }

    if (!std::filesystem::is_directory(path, ec) || ec) {
        error = "data directory is not a directory: '" + path + "'";
        return false;
    }

    const auto probe = std::filesystem::path(path) / ".isched_write_probe";
    {
        std::ofstream out(probe.string(), std::ios::app);
        if (!out) {
            error = "data directory is not writable: '" + path + "'";
            return false;
        }
    }
    std::filesystem::remove(probe, ec);

    return true;
}

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

    if (!is_valid_email_address(admin_name)) {
        error = "--seed admin name must be a valid email address";
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
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--data-dir=") {
            error = "--data-dir requires a non-empty value";
            return std::nullopt;
        }
        if (arg == "--seed=") {
            error = "--seed requires a non-empty value in format <admin_name>:<password>";
            return std::nullopt;
        }
    }

    try {
        po::options_description desc("isched_srv options");
        desc.add_options()
            ("help,h", "show help")
            ("data-dir", po::value<std::string>(), "work directory")
            ("seed", po::value<std::string>(), "seed credentials");

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv)
                      .options(desc)
                      .style(po::command_line_style::default_style |
                             po::command_line_style::allow_long_disguise)
                      .run(),
                  vm);
        po::notify(vm);

        CliOptions opts;
        opts.help_requested = vm.contains("help");

        if (vm.contains("data-dir")) {
            const auto data_dir = vm["data-dir"].as<std::string>();
            if (data_dir.empty()) {
                error = "--data-dir requires a non-empty value";
                return std::nullopt;
            }
            opts.data_dir = data_dir;
        }

        if (vm.contains("seed")) {
            const auto parsed = parse_seed_credentials(vm["seed"].as<std::string>(), error);
            if (!parsed) {
                return std::nullopt;
            }
            opts.seed_credentials = *parsed;
        }

        return opts;
    } catch (const po::invalid_command_line_syntax& ex) {
        const std::string_view what = ex.what();
        if (what.find("--data-dir") != std::string_view::npos) {
            error = "--data-dir requires a value";
        } else if (what.find("--seed") != std::string_view::npos) {
            error = "--seed requires a value in format <admin_name>:<password>";
        } else {
            error = ex.what();
        }
        return std::nullopt;
    } catch (const po::error& ex) {
        error = ex.what();
        return std::nullopt;
    }
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



