#include <cstdlib>
#include <filesystem>
#include <string>
#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>

#include "isched_DocRootResolver.hpp"
#include "isched_EHttpMethods.hpp"
#include "isched_MainSvc.hpp"
#include "isched_SingleActionResolver.hpp"


int main(const int, const char **) {
    using isched::v0_0_1::MainSvc;
    using isched::v0_0_1::DocRootResolver;
    using isched::v0_0_1::SingleActionResolver;
    using isched::v0_0_1::EHttpMethods;
    using std::filesystem::path;
    using std::string;
    using std::make_shared;

    spdlog::cfg::load_env_levels();
    MainSvc mySvc;
    mySvc.addResolver
    (make_shared<SingleActionResolver>
        (EHttpMethods::GET, "/test", "Resolver ansver"));
    mySvc.addResolver
    (make_shared<DocRootResolver>
        (string("/path"), path{"/home/grobap/Documents/100_Projects/isched/isched/client"}));
    spdlog::debug("run ->");
    mySvc.run();
    return EXIT_SUCCESS;
}
