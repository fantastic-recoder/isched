//
// Created by groby on 2025-11-03.
//

#ifndef ISCHED_ISCHED_GQL_GRAMMAR_HPP
#define ISCHED_ISCHED_GQL_GRAMMAR_HPP

#include <tuple>
#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/parse_tree_to_dot.hpp>
#include <tao/pegtl/contrib/trace.hpp>
#include <spdlog/spdlog.h>
#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <regex>
#include <vector>
#include <map>
#include <set>
#include <algorithm>

namespace isched::v0_0_1 {
    namespace pegtl = tao::pegtl;
    using pegtl::one;
    using pegtl::seq;
    using pegtl::star;
    using pegtl::ranges;
    using pegtl::sor;
    using pegtl::must_if;
    using pegtl::opt;
    using pegtl::at;
    using pegtl::plus;
    using pegtl::parse_tree::node;

    using std::endl;
    using std::cout;
    using std::cerr;
    using std::unique_ptr;

    struct Ws : one<' ', '\t', '\n', '\r'> {
    };

    struct HashComment : seq<
                one<'#'>,
                tao::pegtl::until<tao::pegtl::eolf>
            > {
    };

    struct Beg : one<'{'> {
    };

    struct End : one<'}'> {
    };

    struct GqlName : seq<
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

    struct TSeparator : sor<Ws, HashComment> {
    }; // either/or

    struct TSeps : star<TSeparator> {
    }; // any separators, whitespace or comments

    // template to generate rule
    // tao::pegtl::seq<rule0, separator, rule1, separator, rule2, ... , separator, rulen>
    template<typename TSeparator, typename... TRules>
    struct SeqWithComments;

    template<typename TSeparator, typename TRule0, typename... TRulesRest>
    struct SeqWithComments<TSeparator, TRule0, TRulesRest...>
            : seq<TRule0, TSeparator, SeqWithComments<TSeparator, TRulesRest...> > {
    };

    template<typename TSeparator, typename TRule0>
    struct SeqWithComments<TSeparator, TRule0>
            : seq<TRule0, TSeparator> {
    };

    struct GqlSubQuery : SeqWithComments<
                TSeps,
                Beg,
                opt<GqlName>,
                opt<GqlSubQuery>,
                End
            > {
    };

    struct GqlQuery : SeqWithComments<
                TSeps,
                opt<pegtl::string<'q', 'u', 'e', 'r', 'y'> >,
                GqlSubQuery
            > {
    };

    struct GqlTypeName : SeqWithComments<
                TSeps,
                pegtl::plus< pegtl::alpha >,
                TSeps
    >{};

    struct GqlTypeString : SeqWithComments<
                TSeps,
                pegtl::string<'S','t','r','i','n','g'>,
                TSeps
    >{};

    struct GqlTypeInt : SeqWithComments<
                TSeps,
                pegtl::string<'I','n','t'>,
                TSeps
    >{};


    struct GqlTypeField : SeqWithComments<
                TSeps,
                GqlTypeName,
                pegtl::string<':'>,
                sor<GqlTypeString,GqlTypeInt>
    >{};

    struct GqlTypeFields : SeqWithComments<
        TSeps,
        Beg,
        plus<GqlTypeField>,
        End
    >{};

    struct GqlType : seq<
                TSeps,
                pegtl::string<'t', 'y', 'p', 'e'>,
                plus<TSeparator>,
                GqlTypeName,
                GqlTypeFields
            > {
    };

    struct GqlGrammar : plus<
        sor<
                GqlQuery,
                GqlType
        >> {
    };

    template<typename TRule>
    using GqlSelector = pegtl::parse_tree::selector<
        TRule,
        pegtl::parse_tree::store_content::on<
            GqlQuery, GqlName, GqlType, GqlTypeField,GqlTypeName,GqlTypeString,GqlTypeInt
        >
    >;

    //template<typename> inline constexpr const char *error_message = nullptr;
    //template<> inline constexpr auto error_message<Beg> = "expected {";
    //template<> inline constexpr auto error_message<End> = "incomplete query, expected }";
    //template<> inline constexpr auto error_message<name> = "GqlError parsing name";

    //struct GqlError {
    //    template<typename Rule>
    //    static constexpr auto message = error_message<Rule>;
    //};

    //template<typename Rule>
    //using control = must_if<GqlError>::control<Rule>;

    constexpr static const char *const K_OUTPUT_SEP =
"************************************************************************";

    template<typename TGrammar>
    std::tuple<bool,std::unique_ptr<node>>
    generate_ast_and_log(const std::string &pName, pegtl::string_input<>& in, bool p_trace_on_success=false) {
        std::unique_ptr<node> myRoot = pegtl::parse_tree::parse<
            TGrammar, GqlSelector /*, ns_pegtl::nothing, control*/
        >(in);
        spdlog::debug("\n\nAST of \"{}\":\n{}", pName, K_OUTPUT_SEP);
        bool myParsingOk;
        if (myRoot) {
            pegtl::parse_tree::print_dot(std::cout, *myRoot);
            std::cout << endl
                    << K_OUTPUT_SEP << endl
                    << endl;
            myParsingOk = true;
            if (p_trace_on_success) {
                in.restart();
                tao::pegtl::complete_trace<TGrammar>(in);
            }
        } else {
            spdlog::error(R"("{}" error / no AST generated!? Input rest:"{}".)", pName,
                          std::string(in.begin(), in.size()));
            in.restart();
            tao::pegtl::complete_trace<TGrammar>(in);
            myParsingOk = false;
        }
        return {myParsingOk,std::move(myRoot)};
    }

}
#endif //ISCHED_ISCHED_GQL_GRAMMAR_HPP