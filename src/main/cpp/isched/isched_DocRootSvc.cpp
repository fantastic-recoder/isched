//
// Created by grobap on 3.2.2024.
//

#include <fstream>
#include <filesystem>

#include "isched_DocRootSvc.hpp"

#include "isched_ExceptionDocPathNotFound.hpp"


namespace isched::v0_0_1 {

    using std::filesystem::path;

    std::string
    DocRootSvc::readFileToString(const std::string& pFilePath) {
        // Check file existence and size
        std::filesystem::path path(pFilePath);
        if (!std::filesystem::exists(path)) {
            throw ExceptionDocPathNotFound("File does not exist: " + pFilePath);
        }
        auto fileSize = std::filesystem::file_size(path);

        // Reserve enough space in the string
        std::string content;
        content.resize(fileSize);

        // Read file into string
        std::ifstream file(pFilePath);
        file.read(content.data(), fileSize);

        return content;
    }

    DocRootSvc::DocRootSvc(MainSvc &pService, const path& pUri, const path& pDocRoot)
    : mMainSvc(pService), mUri(pUri), mDocRoot(pDocRoot)
    {
    }
} // v0_0_1
// isched