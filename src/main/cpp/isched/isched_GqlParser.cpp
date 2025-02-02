//
// Created by grobap on 29.08.23.
//

#include <iostream>
#include <utility>
#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/parse_tree_to_dot.hpp>
#include <tao/pegtl/contrib/trace.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>
#include "isched_GqlParser.hpp"
#include "isched_LogEnvLoader.hpp"

namespace isched {
    namespace v0_0_1 {
        namespace ns_pegtl = tao::pegtl;
        using ns_pegtl::one;
        using ns_pegtl::seq;
        using ns_pegtl::star;
        using ns_pegtl::ranges;
        using ns_pegtl::sor;
        using ns_pegtl::must_if;
        using ns_pegtl::opt;
        using ns_pegtl::at;
        using ns_pegtl::parse_tree::node;

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

        struct TSeps : tao::pegtl::star<TSeparator> {
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
                    opt<ns_pegtl::string<'q', 'u', 'e', 'r', 'y'> >,
                    GqlSubQuery
                > {
        };

        struct GqlType : seq<
                    ns_pegtl::string<'q', 'u', 'e', 'r', 'y'>
                > {
        };

        struct GqlGrammar : sor<
                    GqlQuery,
                    GqlType
                > {
        };

        template<typename TRule>
        using GqlSelector = ns_pegtl::parse_tree::selector<
            TRule,
            ns_pegtl::parse_tree::store_content::on<
                GqlQuery, GqlName
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

        typedef std::unique_ptr<ns_pegtl::parse_tree::node> ParseTreePtr;

        class GdlParserTree: public IGdlParserTree {
        public:
            explicit GdlParserTree(std::string &&pQuery, const std::string &pName);
            ~GdlParserTree() = default;
            bool isParsingOk() const override { return mParsingOk; }

        private:
            ParseTreePtr mRoot;
            bool         mParsingOk;
        };


        /**
         *
         * @param pQuery GraphQL query to be parsed
         * @param pName An identifier, used as additional (debug/output) information,
         *       can be for example a filename.
         * @return true on success.
         */
        std::unique_ptr<IGdlParserTree> GqlParser::parse(std::string &&pQuery, const std::string &pName) {
             unique_ptr<IGdlParserTree> myGdlParserTree
                     =std::make_unique<GdlParserTree>(std::move(pQuery),pName);
             return myGdlParserTree;
        }

        static const char *const K_OUTPUT_SEP =
            "************************************************************************";


        GdlParserTree::GdlParserTree(std::string &&pQuery, const std::string &pName)  {
            spdlog::debug("Parsing query named \"{}\".", pName);
            // Set up the states, here a single std::string as that is
            // what our action requires as additional function argument.
            ns_pegtl::string_input in(std::move(pQuery), "Query");

            try {
                mRoot = ns_pegtl::parse_tree::parse<
                        GqlGrammar, GqlSelector/*, ns_pegtl::nothing, control*/
                >(in);
                spdlog::debug("\n\nAST of \"{}\":\n{}", pName, K_OUTPUT_SEP);
                if (mRoot) {
                    ns_pegtl::parse_tree::print_dot(std::cout, *mRoot);
                    std::cout << endl
                         << K_OUTPUT_SEP << endl
                         << endl;
                    mParsingOk = true;
                } else {
                    spdlog::error("\"{}\" error / no AST generated!?", pName);
                    ns_pegtl::standard_trace<GqlGrammar>(in);
                    mParsingOk = false;
                }
            } catch (const ns_pegtl::parse_error &e) {
                const auto p = e.positions().front();
                std::cerr << e.what() << std::endl
                          << in.line_at(p) << std::endl
                          << std::setw(p.column) << '^' << std::endl;
                mParsingOk =  false;
            }
        }

    } // namespace v0_0_1
}
