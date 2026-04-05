// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_srv_main.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Server process entry point.
 *
 * Sets up signal handling (`SIGINT`, `SIGTERM`) and drives the
 * `isched::v0_0_1::backend::Server` lifecycle (configure → start → wait → stop).
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>


#include "isched_Server.hpp"

namespace {
std::atomic<bool> keep_running{true};

struct CliOptions {
    std::optional<std::string> data_dir;
    bool help_requested{false};
};

void handle_signal(int) {
    keep_running.store(false);
}

void print_usage(const char* program) {
    std::cerr
        << "Usage: " << program << " [--data-dir <path>] [--help]\n"
        << "\n"
        << "Options:\n"
        << "  --data-dir <path>   Override server work directory (tenant data under <path>/tenants)\n"
        << "  --data-dir=<path>   Same as above\n"
        << "  --help              Show this help text\n";
}

std::optional<CliOptions> parse_cli_args(int argc, char** argv) {
    CliOptions opts;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};

        if (arg == "--help" || arg == "-h") {
            opts.help_requested = true;
            continue;
        }

        if (arg == "--data-dir") {
            if (i + 1 >= argc) {
                spdlog::error("--data-dir requires a value");
                return std::nullopt;
            }
            opts.data_dir = argv[++i];
            continue;
        }

        if (arg.starts_with("--data-dir=")) {
            const std::string value{arg.substr(std::string_view{"--data-dir="}.size())};
            if (value.empty()) {
                spdlog::error("--data-dir requires a non-empty value");
                return std::nullopt;
            }
            opts.data_dir = value;
            continue;
        }

        spdlog::error("Unknown option: {}", arg);
        return std::nullopt;
    }

    return opts;
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

/// Apply ISCHED_ environment variables to a server Configuration.
/// Mapping: ISCHED_SERVER_PORT   → config.port
///          ISCHED_SERVER_HOST   → config.host
///          ISCHED_JWT_SECRET_KEY → config.jwt_secret_key
///          ISCHED_MIN_THREADS   → config.min_threads
///          ISCHED_MAX_THREADS   → config.max_threads
void apply_env_config(isched::v0_0_1::backend::Server::Configuration& cfg) {
    if (const char* v = std::getenv("ISCHED_SERVER_PORT");  v && *v) {
        if (int p = std::atoi(v); p > 0 && p < 65536)
            cfg.port = static_cast<uint16_t>(p);
    }
    if (const char* v = std::getenv("ISCHED_SERVER_HOST");  v && *v)
        cfg.host = v;
    if (const char* v = std::getenv("ISCHED_JWT_SECRET_KEY"); v && *v)
        cfg.jwt_secret_key = v;
    if (const char* v = std::getenv("ISCHED_MIN_THREADS"); v && *v) {
        if (int n = std::atoi(v); n > 0)
            cfg.min_threads = static_cast<std::size_t>(n);
    }
    if (const char* v = std::getenv("ISCHED_MAX_THREADS"); v && *v) {
        if (int n = std::atoi(v); n > 0)
            cfg.max_threads = static_cast<std::size_t>(n);
    }
}
}

int main(int argc, char** argv) {
    using isched::v0_0_1::backend::Server;
    using namespace std::chrono_literals;

    spdlog::cfg::load_env_levels();

    const auto cli_opts = parse_cli_args(argc, argv);
    if (!cli_opts) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (cli_opts->help_requested) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    std::ignore = std::signal(SIGINT, handle_signal);
    std::ignore = std::signal(SIGTERM, handle_signal);

    Server::Configuration config;
    apply_env_config(config);

    // CLI overrides environment/defaults for deterministic test setup.
    if (cli_opts->data_dir) {
        std::string validation_error;
        if (!validate_data_dir(*cli_opts->data_dir, validation_error)) {
            spdlog::error("Invalid --data-dir: {}", validation_error);
            return EXIT_FAILURE;
        }
        config.work_directory = *cli_opts->data_dir;
    }

    auto server = Server::create(config);
    if (!server->start()) {
        spdlog::error("Failed to start GraphQL server");
        return EXIT_FAILURE;
    }

    // Wire the shutdown mutation to the main-loop sentinel so that
    // `mutation { shutdown }` exits cleanly without SIGTERM.
    server->set_shutdown_callback([&keep_running]() {
        keep_running.store(false, std::memory_order_relaxed);
    });

    spdlog::info("Data directory:      {}", server->get_configuration().work_directory);
    spdlog::info(
        "GraphQL endpoint available at http://{}:{}{}",
        server->get_configuration().host,
        server->get_configuration().port,
        server->get_graphql_endpoint_path());
    spdlog::info(
        "Admin UI:           http://{}:{}/graphql",
        server->get_configuration().host,
        server->get_configuration().port);

    // Indicate bootstrap/seed mode in the startup log so operators know
    // the server is waiting for the first platform admin to be created.
    if (server->is_seed_mode_active()) {
        spdlog::info("*** BOOTSTRAP MODE: No platform administrator exists. "
                     "Open the Admin UI to create the initial admin account.");
    }

    while (keep_running.load()) {
        std::this_thread::sleep_for(250ms);
    }

    server->stop();
    return EXIT_SUCCESS;
}
