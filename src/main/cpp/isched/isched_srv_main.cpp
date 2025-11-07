#include <cstdlib>
#include <filesystem>
#include <string>
#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>

#include "isched_doc_root_resolver.hpp"
#include "isched_e_http_methods.hpp"
#include "isched_main_svc.hpp"
#include "isched_single_action_resolver.hpp"


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
        (string("/path"), path{"../../../../docs"}));

    spdlog::debug("run ->");
    mySvc.run();
    return EXIT_SUCCESS;
}
