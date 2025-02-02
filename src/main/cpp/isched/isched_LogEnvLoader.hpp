//
// Created by groby on 2/2/25.
//

#ifndef ISCHED_LOGENVLOADER_H
#define ISCHED_LOGENVLOADER_H

namespace {
    struct LogEnvLoader {
        LogEnvLoader() {
            spdlog::cfg::load_env_levels();
            spdlog::info("Log environment loaded.");
        }
    } theLoader;
}

#endif //ISCHED_LOGENVLOADER_H
