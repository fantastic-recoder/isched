//
// Created by grobap on 29.08.23.
//

#ifndef ISCHED_GRAPHQLPARSER_HPP
#define ISCHED_GRAPHQLPARSER_HPP

#include <string>
#include <memory>

namespace tao {
    namespace pegtl {
        struct node;
    }
}
namespace isched {
    namespace v0_0_1 {

        class GraphQlParser {
        public:
            bool parse(std::string );
        private:
            //std::unique_ptr<tao::pegtl::node> mRoot;
        };
    }
}


#endif //ISCHED_GRAPHQLPARSER_HPP
