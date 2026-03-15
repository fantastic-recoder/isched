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
}

int main(const int, const char**) {
    using isched::v0_0_1::backend::Server;
    using namespace std::chrono_literals;

    spdlog::cfg::load_env_levels();

    std::ignore = std::signal(SIGINT, handle_signal);
    std::ignore = std::signal(SIGTERM, handle_signal);

    auto server = Server::create();
    if (!server->start()) {
        spdlog::error("Failed to start GraphQL server");
        return EXIT_FAILURE;
    }

    spdlog::info(
        "GraphQL endpoint available at http://{}:{}{}",
        server->get_configuration().host,
        server->get_configuration().port,
        server->get_graphql_endpoint_path());

    while (keep_running.load()) {
        std::this_thread::sleep_for(250ms);
    }

    server->stop();
    return EXIT_SUCCESS;
}
