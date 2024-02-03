//
// Created by grobap on 3.2.2024.
//

#ifndef ISCHED_EXCEPTIONBASE_HPP
#define ISCHED_EXCEPTIONBASE_HPP


namespace isched::v0_0_1 {

    class ExceptionBase {
    public:
        virtual ~ExceptionBase() = default;

        // Return the error message
        [[nodiscard]] virtual const char* what() const noexcept = 0;

        // (optional) Return an error code associated with this exception
        [[nodiscard]] virtual int code() const noexcept { return 0; }
    };
}


#endif //EXCEPTIONBASE_HPP
