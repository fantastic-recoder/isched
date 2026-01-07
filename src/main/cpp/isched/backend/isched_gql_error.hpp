//
// Created by groby on 2025-12-29.
//

#ifndef ISCHED_E_ERROR_CODES_HPP
#define ISCHED_E_ERROR_CODES_HPP

#include <string>
#include <vector>

namespace isched::v0_0_1::gql {
    enum class EErrorCodes {
        OK = 0,
        UNKNOWN_ERROR = 1,
        MISSING_GQL_RESOLVER = 2, PARSE_ERROR = 3, EXECUTABLE_DEF_NOT_ALLOWED =4, ARGUMENT_ERROR
    };

    struct Error {
        EErrorCodes code = EErrorCodes::UNKNOWN_ERROR;
        std::string message{};
    };

    using TErrorVector = std::vector<Error>;

}

#endif //ISCHED_E_ERROR_CODES_HPP