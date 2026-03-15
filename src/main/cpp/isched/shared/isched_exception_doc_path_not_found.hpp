// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_exception_doc_path_not_found.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Exception thrown when a requested documentation path does not exist.
 *
 * `ExceptionDocPathNotFound` carries the missing filesystem path and an
 * fmt-formatted error message.  Used by the legacy doc-root resolver layer.
 */

#ifndef ISCHED_DOCPATHNOTFOUND_HPP
#define ISCHED_DOCPATHNOTFOUND_HPP
#include <filesystem>
#include <stdexcept>
#include <fmt/core.h>
#include <string>

#include "isched_exception_base.hpp"


namespace isched::v0_0_1 {
    class ExceptionDocPathNotFound : public ExceptionBase, public std::runtime_error {
        std::filesystem::path path_;
        std::string message_;
        int error_code_;

    public:
        explicit ExceptionDocPathNotFound(const std::filesystem::path &pPath, int error_code = 0)
            // NOLINTNEXTLINE(bugprone-throw-keyword-missing) -- base class constructor call, not a standalone exception object
            : std::runtime_error(fmt::format("Path \"{}\" does not exists! Current directory is: \"{}\".",
                pPath.string(),std::filesystem::current_path().string()))
              , path_(pPath)
              , message_(std::runtime_error::what())
              , error_code_(error_code) {
        }


        const char *what() const noexcept override { return message_.c_str(); }
        int code() const noexcept override { return error_code_; }
        const std::filesystem::path &path() const { return path_; }
    };
}

// v0_0_1
// isched

#endif //DOCPATHNOTFOUND_HPP
