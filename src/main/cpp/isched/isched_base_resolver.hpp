//
// Created by grobap on 4.2.2024.
//

#ifndef BASERESOLVER_HPP
#define BASERESOLVER_HPP
#include <string>


namespace isched::v0_0_1 {
    /**
     * \brief base class for all resolvers.
    */
    class BaseResolver {
    public:
        virtual ~BaseResolver() = default;

        virtual std::string &getPath() = 0;

        virtual std::string &getMethod() = 0;

        virtual std::string handle(std::string&&) = 0;
    };
}

// v0_0_1::isched

#endif //BASERESOLVER_HPP
