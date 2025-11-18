/**
 * @file isched_gql_parser.cpp
 * @brief Implementation of the GraphQL PEGTL parser facade.
 */

#include <iostream>
#include <utility>
#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>
#include "isched_gql_parser.hpp"
#include "isched_log_env_loader.hpp"
#include "isched_gql_grammar.hpp"

namespace isched::v0_0_1 {

    using ParseTreePtr = std::unique_ptr<tao::pegtl::parse_tree::node>;

    /**
     * @brief Concrete parse result holding the PEGTL parse tree.
     * The object answers `isParsingOk()` and keeps the root `node` alive as needed.
     */
    class GdlParserTree : public IGdlParserTree {
    public:
        /**
         * @brief Construct a parse-result by parsing the provided input against @ref Document.
         * @param pQuery GraphQL input string (moved into an internal PEGTL input).
         * @param pName  A label used in debug/trace output (e.g., file name).
         */
        explicit GdlParserTree(std::string &&pQuery, const std::string &pName);

        ~GdlParserTree() = default;

        bool isParsingOk() const override { return mParsingOk; }


    private:
        ParseTreePtr mRoot;   ///< Root of the parse tree (non-null on success)
        bool mParsingOk;      ///< `true` if parsing succeeded
    };


    /// Parse a single GraphQL input with the grammar entry point @ref Document.
    std::unique_ptr<IGdlParserTree> GqlParser::parse(std::string &&pQuery, const std::string &pName) {
        std::unique_ptr<IGdlParserTree> myGdlParserTree
                = std::make_unique<GdlParserTree>(std::move(pQuery), pName);
        return myGdlParserTree;
    }


    GdlParserTree::GdlParserTree(std::string &&pQuery, const std::string &pName) {
        spdlog::debug("Parsing query named \"{}\".", pName);
        // Set up the states, here a single std::string as that is
        // what our action requires as additional function argument.
        tao::pegtl::string_input in(std::move(pQuery), "Query");

        try {
            auto myRetVal = gql::generate_ast_and_log<gql::Document>(pName, in);
            mRoot = std::move(std::get<1>(myRetVal));
            mParsingOk = std::get<0>(myRetVal);
        } catch (const tao::pegtl::parse_error &e) {
            const auto p = e.positions().front();
            std::cerr << e.what() << std::endl
                    << in.line_at(p) << std::endl
                    << std::setw(static_cast<int>(p.column)) << '^' << std::endl;
            mParsingOk = false;
        }
    }
} // namespace isched::v0_0_1
