#include <future>
#include <catch2/catch_test_macros.hpp>
#include <isched/isched.hpp>
#include "isched/isched_GqlParser.hpp"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"
#include "cpp-subprocess/subprocess.hpp"

#include "isched/isched_LogEnvLoader.hpp"

using namespace subprocess;
using slaunch = std::launch;

using isched::v0_0_1::GqlParser;

namespace {
    auto theSrvLambda = []() {
        spdlog::debug("starting async" );
        auto out_buffer = check_output({"./rest_hello_world"});
        std::string mySrvOutput(out_buffer.buf.data(), out_buffer.buf.size());
        spdlog::debug( "Server printed: \"{}\"\n", mySrvOutput);
        return mySrvOutput;
    };
}

TEST_CASE( "Isched server", "[hello_world]" ) {
    auto fut = std::async(slaunch::async,theSrvLambda); // "fut" for "future"
    fut.wait_for(std::chrono::seconds(1));
    spdlog::debug("server starting async" );
    httplib::Client cli("http://localhost:1984");
    auto res=cli.Post("/resource", "John","text/plain");
    REQUIRE(res);
    spdlog::debug("Request status: {}; body: \"{}\"\n",res->status, res->body);
    cli.Post("/resource", "exit","text/plain");
    REQUIRE(fut.wait_for(std::chrono::seconds(30))==std::future_status::ready);
}