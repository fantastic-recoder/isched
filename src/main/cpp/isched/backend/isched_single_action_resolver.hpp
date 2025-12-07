//
// Created by grobap on 4.2.2024.
//

#ifndef ISCHED_SINGLEACTIONRESOLVER_HPP
#define ISCHED_SINGLEACTIONRESOLVER_HPP

#include "isched_BaseRestResolver.hpp"
#include "isched_e_http_methods.hpp"


namespace isched::v0_0_1 {

class SingleActionResolver: public BaseRestResolver {
    EHttpMethods mMethod = EHttpMethods::GET;
    std::string mMethodStr ="GET";
    std::string mPath = "/";
    std::string mAnsver = "Hello world from resolver!";

public:
    SingleActionResolver() = delete;
    SingleActionResolver(EHttpMethods pMethod, const std::string& pPath, std::string&&  pAnsver);
    ~SingleActionResolver() override;

    std::string & getPath() override { return mPath; }

    std::string & getMethod() override { return mMethodStr; }

    std::string handle(std::string&& ) override {
        return mAnsver;
    }
};

}
// isched:: v0_0_1

#endif //ISCHED_SINGLEACTIONRESOLVER_HPP
