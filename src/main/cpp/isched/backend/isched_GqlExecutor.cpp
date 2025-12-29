//
// Created by groby on 2025-12-19.
//

#include "isched_GqlExecutor.hpp"

#include <regex>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <tao/pegtl.hpp>
#include <isched/backend/isched_GqlParser.hpp>

#include "isched_gql_grammar.hpp"

namespace isched::v0_0_1::backend {
    using nlohmann::json;

    GqlExecutor::GqlExecutor(std::shared_ptr<DatabaseManager> database, Config config) {
    }

    void GqlExecutor::setup_builtin_resolvers() {
    }

    ExecutionResult GqlExecutor::load_schema(const std::string &&string) {
        ExecutionResult result;
        return result;
    }

    std::pair<DocumentPtr, std::vector<std::string> > GqlExecutor::parse(std::string &&pQuery) const {
        if (pQuery.length() > 100000) {
            // Max pQuery length
            return {nullptr, {"Query length exceeds maximum allowed"}};
        }
        static const std::string aName = "ExecutorQuery";
        // Set up the states, here a single std::string as that is
        // what our action requires as an additional function argument.
        tao::pegtl::string_input in(std::move(pQuery), aName);
        bool aParsingOk = false;
        try {
            auto myRetVal = gql::generate_ast_and_log<gql::Document>(in, aName);
            auto aRoot = std::move(std::get<1>(myRetVal));
            aParsingOk = std::get<0>(myRetVal);
        } catch (const tao::pegtl::parse_error &e) {
            const auto p = e.positions().front();
            std::cerr << e.what() << std::endl
                    << in.line_at(p) << std::endl
                    << std::setw(static_cast<int>(p.column)) << '^' << std::endl;
        }

        auto document = std::make_shared<gql::Document>();
        std::vector<std::string> errors;

        // Simple regex-based parsing for basic queries
        // This is a minimal implementation for demonstration
        /*
        std::regex query_regex(R"(\s*\{\s*(\w+)\s*\}\s*)");
        std::smatch matches;

        if (std::regex_match(pQuery, matches, query_regex)) {
            auto operation = std::make_shared<OperationDefinition>(OperationType::Query);

            std::string field_name = matches[1].str();
            auto field = std::make_shared<Field>(field_name);
            auto field_selection = std::make_shared<FieldSelection>(field);
            operation->selection_set.push_back(field_selection);

            document->operations.push_back(operation);
        } else {
            // Handle more complex queries with nested structure
            std::regex complex_query_regex(R"(\s*\{\s*(\w+)\s*\{\s*(\w+)\s*\}\s*\}\s*)");
            if (std::regex_match(pQuery, matches, complex_query_regex)) {
                auto operation = std::make_shared<OperationDefinition>(OperationType::Query);

                std::string parent_field = matches[1].str();
                std::string nested_field = matches[2].str();

                auto parent_field_obj = std::make_shared<Field>(parent_field);
                auto nested_field_obj = std::make_shared<Field>(nested_field);
                auto nested_selection = std::make_shared<FieldSelection>(nested_field_obj);

                parent_field_obj->selection_set.push_back(nested_selection);
                auto parent_selection = std::make_shared<FieldSelection>(parent_field_obj);
                operation->selection_set.push_back(parent_selection);

                document->operations.push_back(operation);
            } else {
                errors.push_back("Failed to parse pQuery: unsupported syntax");
            }
        }
        */

        return {document, errors};
    }

    ExecutionResult GqlExecutor::execute(const std::string &p_query, const nlohmann::json &p_variables,
                                         const std::string &p_operation_name, const nlohmann::json &p_context) const {
        return ExecutionResult{};
    }

    ExecutionResult GqlExecutor::execute(gql::Document p_doc) {
        return ExecutionResult{};
    };

    std::unique_ptr<GqlExecutor> GqlExecutor::create(std::shared_ptr<DatabaseManager> database) {
        return std::make_unique<GqlExecutor>(std::move(database));
    }
}
