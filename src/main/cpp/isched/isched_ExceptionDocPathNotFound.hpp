//
// Created by grobap on 3.2.2024.
//

#ifndef ISCHED_DOCPATHNOTFOUND_HPP
#define ISCHED_DOCPATHNOTFOUND_HPP
#include <filesystem>
#include <stdexcept>
#include <fmt/core.h>
#include <string>

#include "isched_ExceptionBase.hpp"


namespace isched::v0_0_1 {
    class ExceptionDocPathNotFound : public ExceptionBase, public std::runtime_error {
        std::filesystem::path path_;
        std::string message_;
        int error_code_;

    public:
        explicit ExceptionDocPathNotFound(const std::filesystem::path &pPath, int error_code = 0)
            : std::runtime_error(fmt::format("Path \"{}\" does not exists!", pPath.string()))
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
