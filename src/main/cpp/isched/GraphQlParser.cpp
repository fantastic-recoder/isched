//
// Created by grobap on 29.08.23.
//

#include <iostream>
#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/parse_tree.hpp>
#include <tao/pegtl/contrib/parse_tree_to_dot.hpp>
#include "GraphQlParser.hpp"


namespace isched {
    namespace v0_0_1 {

        namespace ns_pegtl = tao::pegtl;
        using ns_pegtl::one;
        using ns_pegtl::seq;
        using ns_pegtl::star;
        using ns_pegtl::ranges;
        using ns_pegtl::sor;
        using ns_pegtl::must_if;

        struct ws : one<' ', '\t', '\n', '\r'> {
        };

        struct beg : seq<
                star<ws>,
                one<'{'>,
                star<ws>> {
        };

        struct end : seq<
                star<ws>,
                one<'}'>,
                star<ws>> {
        };

        struct name : seq<
                sor<
                        one<'_'>,
                        ranges<'A', 'Z', 'a', 'z'>
                >,
                star<
                        sor<
                                one<'_'>,
                                ranges<'0', '9', 'A', 'Z', 'a', 'z'>
                        >
                >
        > {
        };

        struct query : seq<beg, star<name>, end> {
        };

        template<typename Rule>
        using selector = ns_pegtl::parse_tree::selector<
                Rule,
                ns_pegtl::parse_tree::store_content::on<
                        name>
        >;

        template<typename> inline constexpr const char *error_message = nullptr;
        template<> inline constexpr auto error_message<beg> = "expected {";
        template<> inline constexpr auto error_message<end> = "incomplete query, expected }";
        //template<> inline constexpr auto error_message<name> = "error parsing name";

        struct error {
            template<typename Rule>
            static constexpr auto message = error_message<Rule>;
        };

        template<typename Rule>
        using control = must_if<error>::control<Rule>;

        bool GraphQlParser::parse(std::string pQuery) {
            // Set up the states, here a single std::string as that is
            // what our action requires as additional function argument.
            ns_pegtl::string_input in(pQuery, "Query");

            try {
                const auto root = ns_pegtl::parse_tree::parse<query, selector, ns_pegtl::nothing, control>(in);
                ns_pegtl::parse_tree::print_dot(std::cout, *root);
            }
            catch (const ns_pegtl::parse_error &e) {
                const auto p = e.positions().front();
                std::cerr << e.what() << std::endl
                          << in.line_at(p) << std::endl
                          << std::setw(p.column) << '^' << std::endl;
                return false;
            }
            return true;
        }
    }
}