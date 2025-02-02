//
// Created by grobap on 4.2.2024.
//

#ifndef ISCHED_EXCEPTIONUNKNOWNENUMVALUE_HPP
#define ISCHED_EXCEPTIONUNKNOWNENUMVALUE_HPP
#include <stdexcept>

#include "isched_ExceptionBase.hpp"

namespace isched::v0_0_1 {
    class ExceptionUnknownEnumValue final : public ExceptionBase, public std::runtime_error {

        std::string message{};
        int error_code;

    public:
        explicit ExceptionUnknownEnumValue(const std::string& message, int error_code = 0)
        : std::runtime_error(message), message(message), error_code(error_code) {
        }


        [[nodiscard]] const char* what() const noexcept override { return message.c_str(); }
        [[nodiscard]] int code() const noexcept override { return error_code; }

    };
}
#endif //ISCHED_EXCEPTIONUNKNOWNENUMVALUE_HPP
