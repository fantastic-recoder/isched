// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_test_run_listener.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Catch2 event listener that logs test run start/end via spdlog.
 *
 * Registers `TestRunListener` to emit a spdlog info message when the
 * Catch2 test run begins and ends, providing timestamps in CI output.
 */

#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <spdlog/spdlog.h>

class TestRunListener : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;

    // This runs once before any test cases start
    void testRunStarting(Catch::TestRunInfo const&) override {
        spdlog::set_level(spdlog::level::debug);
        spdlog::info("Global Setup: spdlog initialized to DEBUG");
    }
};

CATCH_REGISTER_LISTENER(TestRunListener)
