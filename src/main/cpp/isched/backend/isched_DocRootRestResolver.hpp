// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_DocRootRestResolver.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Resolver serving static documentation from a configured doc root.
 */

#ifndef ISCHED_DOCROOTSVC_HPP
#define ISCHED_DOCROOTSVC_HPP

#include <filesystem>

#include "isched_BaseRestResolver.hpp"
#include "isched/backend/isched_EHttpMethods.hpp"

namespace isched::v0_0_1 {

    class DocRootRestResolver : public BaseRestResolver {
    public:
        std::string & getPath() override { return mPath; }

        std::string & getMethod() override {return mMethod; }

        std::string handle(std::string&& pString) override;

    private:
        std::filesystem::path mDocRoot;
        std::string mPath;
        std::string mDocRootStr;
        std::string mMethod;
        std::string readFileToString(const std::string& pFilePath);
    public:
        DocRootRestResolver(std::string  pPath, const std::filesystem::path& pDocRoot);
    };

} // isched::v0_0_1


#endif //ISCHED_DOCROOTSVC_HPP
