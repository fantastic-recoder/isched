//
// Created by grobap on 4.2.2024.
//

#include "isched_SingleActionResolver.hpp"


namespace isched::v0_0_1 {
    SingleActionResolver::SingleActionResolver(const EHttpMethods pMethod, const std::string &pPath,
        std::string&& pAnsver) : mMethod(pMethod),
    mMethodStr(toString(pMethod)),
    mPath(pPath),
    mAnsver(std::move(pAnsver))
    {
    }

    SingleActionResolver::~SingleActionResolver() = default;
} // isched::v0_0_1
