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
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>


#include "isched_Server.hpp"
#include "isched_srv_cli.hpp"

namespace {
std::atomic<bool> keep_running{true};

void handle_signal(int) {
    keep_running.store(false);
}

/// Apply ISCHED_ environment variables to a server Configuration.
/// Mapping: ISCHED_SERVER_PORT   → config.port
///          ISCHED_SERVER_HOST   → config.host
///          ISCHED_JWT_SECRET_KEY → config.jwt_secret_key
///          ISCHED_MIN_THREADS   → config.min_threads
///          ISCHED_MAX_THREADS   → config.max_threads
///          ISCHED_ALLOWED_ORIGINS → exact comma-separated browser Origin allowlist
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
    if (const char* v = std::getenv("ISCHED_ALLOWED_ORIGINS"); v) {
        cfg.allowed_origins.clear();
        std::istringstream input(v);
        std::string origin;
        while (std::getline(input, origin, ',')) {
            const auto first = origin.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) continue;
            const auto last = origin.find_last_not_of(" \t\r\n");
            cfg.allowed_origins.push_back(origin.substr(first, last - first + 1));
        }
    }
}
}

int main(int argc, char** argv) {
    using isched::v0_0_1::backend::Server;
    using namespace isched::v0_0_1::backend::srvcli;
    using namespace std::chrono_literals;

    spdlog::cfg::load_env_levels();

    std::string cli_error;
    const auto cli_opts = parse_cli_args(argc, argv, cli_error);
    if (!cli_opts) {
        spdlog::error("{}", cli_error);
        std::cerr << build_usage_text(argv[0]);
        return EXIT_FAILURE;
    }
    if (cli_opts->help_requested) {
        std::cout << build_usage_text(argv[0]);
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

    if (cli_opts->seed_credentials) {
        if (!prompt_seed_confirmation(std::cin, std::cout)) {
            spdlog::warn("Seed reset aborted: expected exact confirmation 'yes'");
            return EXIT_FAILURE;
        }

        std::string seed_error;
        const auto seed_result = reset_and_seed_platform_db(
            config.work_directory, *cli_opts->seed_credentials, seed_error);
        if (!seed_result) {
            spdlog::error("Seed reset failed: {}", seed_error);
            return EXIT_FAILURE;
        }

        if (seed_result->backup_path) {
            spdlog::info("Platform DB backup: {}", seed_result->backup_path->string());
        }
        spdlog::info("Platform DB reset complete and bootstrap admin credentials seeded");
    }

    auto server = Server::create(config);
    if (!server->start()) {
        spdlog::error("Failed to start GraphQL server");
        return EXIT_FAILURE;
    }

    // Wire the shutdown mutation to the main-loop sentinel so that
    // `mutation { shutdown }` exits cleanly without SIGTERM.
    server->set_shutdown_callback([]() {
        keep_running.store(false, std::memory_order_relaxed);
    });

    spdlog::info("Data directory:      {}", server->get_configuration().work_directory);
    spdlog::info(
        "GraphQL endpoint available at http://{}:{}{}",
        server->get_configuration().host,
        server->get_configuration().port,
        server->get_graphql_endpoint_path());
    spdlog::info(
        "Admin UI:           http://{}:{}/isched",
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
