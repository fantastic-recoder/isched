//
// Created by grobap on 29.08.23.
//

#ifndef ISCHED_GQLPARSER_HPP
#define ISCHED_GQLPARSER_HPP

#include <string>
#include <memory>
#include <tao/pegtl.hpp>

namespace isched {
    namespace v0_0_1 {

        class GqlParser {
        public:
            bool parse(std::string&& pQuery, const std::string& pName );
        private:
        };
    }
}


#endif //ISCHED_GQLPARSER_HPP
