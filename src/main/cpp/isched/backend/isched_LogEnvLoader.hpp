// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_LogEnvLoader.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Static spdlog environment-level loader, applied once at program start.
 *
 * Defines an anonymous-namespace `LogEnvLoader` struct whose constructor
 * calls `spdlog::cfg::load_env_levels()` so that `SPDLOG_LEVEL` and similar
 * environment variables take effect before any logging occurs.
 */

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
