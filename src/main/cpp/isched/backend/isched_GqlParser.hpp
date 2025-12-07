/**
 * @file isched_GqlParser.hpp
 * @brief Public parser interface for the GraphQL PEGTL grammar.
 *
 * This header declares a minimal facade (`GqlParser`) that parses a GraphQL input
 * using the grammar defined in `isched_gql_grammar.hpp` and returns a lightweight
 * result object (`IGdlParserTree`) with the parse status.
 *
 * See also:
 *  - Grammar entry point: @ref isched::v0_0_1::Document
 *  - Implementation: `isched_gql_parser.cpp`
 */

#ifndef ISCHED_GQLPARSER_HPP
#define ISCHED_GQLPARSER_HPP

#include <string>
#include <memory>


namespace isched::v0_0_1 {

    /**
     * @brief Parse result interface.
     * Implementations encapsulate the parse tree (if needed) and report success.
     */
    struct IGdlParserTree {
        [[nodiscard]] virtual bool isParsingOk() const = 0;

        virtual ~IGdlParserTree() = default;
    };

    /**
     * @brief Facade to parse GraphQL inputs using the PEGTL grammar.
     *
     * Typical usage:
     * @code{.cpp}
     * isched::v0_0_1::GqlParser parser;
     * auto res = parser.parse("{ hero }", "example");
     * if (res->isParsingOk()) {
     *     // ...
     * }
     * @endcode
     */
    class GqlParser {
    public:
        /**
         * @brief Parse a GraphQL input string.
         * @param pQuery Input string (moved into PEGTL input).
         * @param pName  Identifier used in diagnostics (e.g., filename or label).
         * @return A parse-result object; call `isParsingOk()` to check success.
         */
        std::unique_ptr<IGdlParserTree> parse(std::string &&pQuery, const std::string &pName);

        ~GqlParser() = default;

    private:

    };

}


#endif //ISCHED_GQLPARSER_HPP
