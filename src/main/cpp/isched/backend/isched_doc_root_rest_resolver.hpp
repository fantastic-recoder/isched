//
// Created by grobap on 3.2.2024.
//

#ifndef ISCHED_DOCROOTSVC_HPP
#define ISCHED_DOCROOTSVC_HPP

#include <filesystem>

#include "isched_BaseRestResolver.hpp"
#include "isched/backend/isched_e_http_methods.hpp"

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
