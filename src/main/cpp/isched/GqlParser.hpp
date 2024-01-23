//
// Created by grobap on 29.08.23.
//

#ifndef ISCHED_GQLPARSER_HPP
#define ISCHED_GQLPARSER_HPP

#include <string>
#include <memory>

namespace tao {
    namespace pegtl {
        namespace parse_tree {
            struct node;
        }
    }
}

namespace isched {
    namespace v0_0_1 {

        struct IGdlParserTree {
            [[nodiscard]] virtual bool isParsingOk() const = 0;
            virtual ~IGdlParserTree() = default;
        };

        class GqlParser {
        public:
            std::unique_ptr<IGdlParserTree> parse(std::string&& pQuery, const std::string& pName );
            ~GqlParser() = default;
        private:
        };
    }
}


#endif //ISCHED_GQLPARSER_HPP
