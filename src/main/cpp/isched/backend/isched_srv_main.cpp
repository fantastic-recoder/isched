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
#include <thread>
#include <utility>

#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>

#include "isched_Server.hpp"

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

int main(const int, const char**) {
    using isched::v0_0_1::backend::Server;
    using namespace std::chrono_literals;

    spdlog::cfg::load_env_levels();

    std::ignore = std::signal(SIGINT, handle_signal);
    std::ignore = std::signal(SIGTERM, handle_signal);

    Server::Configuration config;
    apply_env_config(config);

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

    spdlog::info(
        "GraphQL endpoint available at http://{}:{}{}",
        server->get_configuration().host,
        server->get_configuration().port,
        server->get_graphql_endpoint_path());
    spdlog::info(
        "Admin UI:           http://{}:{}/isched",
        server->get_configuration().host,
        server->get_configuration().port);

    while (keep_running.load()) {
        std::this_thread::sleep_for(250ms);
    }

    server->stop();
    return EXIT_SUCCESS;
}
