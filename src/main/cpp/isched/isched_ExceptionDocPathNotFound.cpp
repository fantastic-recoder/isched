//
// Created by grobap on 3.2.2024.
//

#include "isched_ExceptionDocPathNotFound.hpp"


namespace isched::v0_0_1 {
    ExceptionDocPathNotFound::ExceptionDocPathNotFound(const std::string &message, int error_code)
    : std::runtime_error(message), message(message), error_code(error_code) {
    }
} // isched::v0_0_1