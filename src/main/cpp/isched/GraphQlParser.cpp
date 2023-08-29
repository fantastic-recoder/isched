//
// Created by grobap on 29.08.23.
//

#include <tao/pegtl.hpp>
#include "GraphQlParser.hpp"


namespace isched {
    namespace v0_0_1 {

        namespace pegtl = tao::pegtl;

        struct beg : pegtl::one< '{' >{};
        struct end : pegtl::one< '}' >{};

        struct query : pegtl::seq<beg,end>{};

        // Primary action class template.
        template< typename Rule >
        struct my_action
                : tao::pegtl::nothing< Rule > {};

        // Specialise the action class template.
        template<>
        struct my_action< tao::pegtl::any >
        {
            // Implement an apply() function that will be called by
            // the PEGTL every time tao::pegtl::any matches during
            // the parsing run.
            template< typename ActionInput >
            static void apply( const ActionInput& in, std::string& out )
            {
                // Get the portion of the original input that the
                // rule matched this time as string and append it
                // to the result string.
                out += in.string();
            }
        };
        bool GraphQlParser::parse(std::string pQuery) {
            // Set up the states, here a single std::string as that is
            // what our action requires as additional function argument.
            std::string out;
            pegtl::string_input in(pQuery,"Query");
            // Start the parsing run with our grammar, action and state.
            bool myRetVal=tao::pegtl::parse< query, my_action >( in, out );
            return myRetVal;
        }
    }
}