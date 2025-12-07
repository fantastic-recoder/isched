/**
 * @file isched_DocRootRestResolver.cpp
 * @brief Implementation of resolver serving static documentation from a configured doc root.
 */

#include <fstream>
#include <filesystem>
#include <utility>

#include "isched_DocRootRestResolver.hpp"

#include <spdlog/spdlog.h>

#include "../shared/isched_exception_doc_path_not_found.hpp"


namespace isched::v0_0_1 {
    using std::filesystem::path;
    using std::filesystem::exists;
    using std::filesystem::is_directory;

    std::string DocRootRestResolver::handle(std::string&& pString) {
        spdlog::debug("Resolving \"{}\".",pString);
        return "DocRootResolver of "+pString;
    }

    std::string
    DocRootRestResolver::readFileToString(const std::string &pFilePath) {
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

    DocRootRestResolver::DocRootRestResolver(std::string pPath, const path &pDocRoot)
        : mDocRoot(std::filesystem::canonical(pDocRoot))
          , mPath(std::move(pPath))
          , mDocRootStr(mDocRoot.string())
          , mMethod(toString(EHttpMethods::GET)) {
        if (!exists(mDocRoot)) {
            throw ExceptionDocPathNotFound(mDocRoot);
        }
    }
} // isched::v0_0_1
