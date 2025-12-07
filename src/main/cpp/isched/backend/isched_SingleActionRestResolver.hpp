/**
 * @file isched_SingleActionRestResolver.hpp
 * @brief Resolver that responds to a single REST action with a fixed answer.
 */

#ifndef ISCHED_SINGLEACTIONRESOLVER_HPP
#define ISCHED_SINGLEACTIONRESOLVER_HPP

#include "isched_BaseRestResolver.hpp"
#include "isched_EHttpMethods.hpp"


namespace isched::v0_0_1 {

class SingleActionRestResolver: public BaseRestResolver {
    EHttpMethods mMethod = EHttpMethods::GET;
    std::string mMethodStr ="GET";
    std::string mPath = "/";
    std::string mAnsver = "Hello world from resolver!";

public:
    SingleActionRestResolver() = delete;
    SingleActionRestResolver(EHttpMethods pMethod, const std::string& pPath, std::string&&  pAnsver);
    ~SingleActionRestResolver() override;

    std::string & getPath() override { return mPath; }

    std::string & getMethod() override { return mMethodStr; }

    std::string handle(std::string&& ) override {
        return mAnsver;
    }
};

}
// isched:: v0_0_1

#endif //ISCHED_SINGLEACTIONRESOLVER_HPP
