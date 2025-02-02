//
// Created by grobap on 4.2.2024.
//

#ifndef ISCHED_EHTTPMETHODS_HPP
#define ISCHED_EHTTPMETHODS_HPP

#include <string>

#include "isched_ExceptionUnknownEnumValue.hpp"

namespace isched::v0_0_1 {
    enum class EHttpMethods {
        GET,
        POST
    };

    constexpr  const char* toString(const EHttpMethods pMethod)
    {
        switch(pMethod)
        {
            case EHttpMethods::GET: return "GET";
            case EHttpMethods::POST: return "POST";
        }
        throw new ExceptionUnknownEnumValue("Unknown value in toString conversion of EHttpMethods.");
    }
}

#endif //ISCHED_EHTTPMETHODS_HPP
