//
// Created by grobap on 29.08.23.
//

#include <iostream>
#include <utility>
#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>
#include "isched_gql_parser.hpp"
#include "isched_log_env_loader.hpp"
#include "isched_gql_grammar.hpp"

namespace isched::v0_0_1 {

    using ParseTreePtr = std::unique_ptr<node>;

    class GdlParserTree : public IGdlParserTree {
    public:
        explicit GdlParserTree(std::string &&pQuery, const std::string &pName);

        ~GdlParserTree() = default;

        bool isParsingOk() const override { return mParsingOk; }


    private:
        ParseTreePtr mRoot;
        bool mParsingOk;
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
                = std::make_unique<GdlParserTree>(std::move(pQuery), pName);
        return myGdlParserTree;
    }


    GdlParserTree::GdlParserTree(std::string &&pQuery, const std::string &pName) {
        spdlog::debug("Parsing query named \"{}\".", pName);
        // Set up the states, here a single std::string as that is
        // what our action requires as additional function argument.
        pegtl::string_input in(std::move(pQuery), "Query");

        try {
            auto myRetVal=generate_ast_and_log<Document>(pName, in);
            mRoot = std::move(std::get<1>(myRetVal));
            mParsingOk = std::get<0>(myRetVal);
        } catch (const pegtl::parse_error &e) {
            const auto p = e.positions().front();
            std::cerr << e.what() << std::endl
                    << in.line_at(p) << std::endl
                    << std::setw(static_cast<int>(p.column)) << '^' << std::endl;
            mParsingOk = false;
        }
    }
} // namespace isched::v0_0_1
