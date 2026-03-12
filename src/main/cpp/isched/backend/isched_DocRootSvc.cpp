// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_DocRootSvc.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Implementation of the doc-root static-file service integration (legacy).
 *
 * @deprecated Scheduled for removal in Phase 7 together with the REST transport layer.
 */

#include <fstream>
#include <filesystem>

#include "../isched_doc_root_svc.hpp"

#include "../shared/isched_exception_doc_path_not_found.hpp"


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