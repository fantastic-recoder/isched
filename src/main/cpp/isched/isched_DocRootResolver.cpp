//
// Created by grobap on 3.2.2024.
//

#include <fstream>
#include <filesystem>

#include "isched_DocRootResolver.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "isched_ExceptionDocPathNotFound.hpp"


namespace isched::v0_0_1 {
    using std::filesystem::path;
    using std::filesystem::exists;
    using std::filesystem::is_directory;

    std::string DocRootResolver::handle(std::string&& pString) {
        spdlog::debug("Resolving \"{}\".",pString);
        return "DocRootResolver of "+pString;
    }

    std::string
    DocRootResolver::readFileToString(const std::string &pFilePath) {
        // Check file existence and size
        path const myPath(pFilePath);
        if (!exists(myPath)) {
            throw ExceptionDocPathNotFound(myPath);
        }
        auto mySize = file_size(myPath);

        // Reserve enough space in the string
        std::string content;
        content.resize(mySize);

        // Read file into string
        std::ifstream file(pFilePath);
        file.read(content.data(), mySize);

        return content;
    }

    DocRootResolver::DocRootResolver(const std::string &pPath, const path &pDocRoot)
        : mDocRootStr(mDocRoot.string())
          , mDocRoot(pDocRoot)
          , mPath(pPath)
          , mMethod(toString(EHttpMethods::GET)) {
        if (!exists(mDocRoot)) {
            throw ExceptionDocPathNotFound(pDocRoot);
        }
    }
} // isched::v0_0_1
