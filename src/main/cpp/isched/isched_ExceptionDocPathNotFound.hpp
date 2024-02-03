//
// Created by grobap on 3.2.2024.
//

#ifndef DOCPATHNOTFOUND_HPP
#define DOCPATHNOTFOUND_HPP
#include <stdexcept>

#include "isched_ExceptionBase.hpp"


namespace isched::v0_0_1 {

class ExceptionDocPathNotFound : public ExceptionBase, public std::runtime_error {
private:
    std::string message;
    int error_code;

public:
    explicit ExceptionDocPathNotFound(const std::string& message, int error_code = 0);

    const char* what() const noexcept override { return message.c_str(); }
    int code() const noexcept override { return error_code; }

};

}
// v0_0_1
// isched

#endif //DOCPATHNOTFOUND_HPP
