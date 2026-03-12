// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_DocRootSvc.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Service integration of DocRoot handler with MainSvc.
 */

#ifndef ISCHED_DOCROOTSVC_HPP
#define ISCHED_DOCROOTSVC_HPP

#include <filesystem>


#include "isched_MainSvc.hpp"


namespace isched::v0_0_1 {

    class DocRootSvc {
        MainSvc &mMainSvc;
        std::filesystem::path mDocRoot;
        std::filesystem::path mUri;
        std::string readFileToString(const std::string& pFilePath);
    public:
        DocRootSvc(MainSvc &pService, const std::filesystem::path& pUri, const std::filesystem::path& pDocRoot);
    };

} // isched::v0_0_1


#endif //DOCROOTSVC_HPP
