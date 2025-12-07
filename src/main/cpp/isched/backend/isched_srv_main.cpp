#include <cstdlib>
#include <filesystem>
#include <string>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>

#include "isched_DocRootRestResolver.hpp"
#include "isched_EHttpMethods.hpp"
#include "isched_MainSvc.hpp"
#include "isched_SingleActionRestResolver.hpp"

void add_safely_resolver(isched::v0_0_1::MainSvc& mySvc) {
    try {
        auto doc_root_resolver = make_shared<isched::v0_0_1::DocRootRestResolver>
                (std::string("/path"), std::filesystem::path{"../../../../../docs"});
        mySvc.addResolver(doc_root_resolver);
    } catch (const std::exception& e) {
        spdlog::error("Failed to add resolver: {}", e.what());
    }
}

int main(const int, const char **) {
    using isched::v0_0_1::MainSvc;
    using isched::v0_0_1::DocRootRestResolver;
    using isched::v0_0_1::SingleActionRestResolver;
    using isched::v0_0_1::EHttpMethods;
    using std::filesystem::path;
    using std::string;
    using std::make_shared;

    spdlog::cfg::load_env_levels();
    MainSvc mySvc;
    mySvc.addResolver
    (make_shared<SingleActionRestResolver>
        (EHttpMethods::GET, "/test", "Resolver ansver"));
    add_safely_resolver(mySvc);
    spdlog::debug("run ->");
    mySvc.run();
    return EXIT_SUCCESS;
}
