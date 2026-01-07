//
// Created by groby on 2026-01-04.
//
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
