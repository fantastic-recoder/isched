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

    void GqlExecutor::process_field_definition(ExecutionResult& p_result,
                                               const TNodePtr &p_typedef, size_t p_idx) {
        const auto& myField = p_typedef->children[p_idx];
        if (myField->children.empty() ) {
            p_result.errors.push_back({
                EErrorCodes::PARSE_ERROR,"Incomplete Query field definition"});
        } else {
            const auto myFieldName = myField->children[0]->string_view();
            spdlog::debug("Checking resolver for field {} in Query type", myFieldName);
            if (!m_resolvers.has_resolver(std::string(myFieldName))) {
                p_result.errors.push_back({
                    EErrorCodes::MISSING_GQL_RESOLVER,
                    std::format("Missing resolver for field {} in Query type", myFieldName)
                });
            }
        }
    }

    json GqlExecutor::process_arguments(const TNodePtr &p_field_node, ExecutionResult &p_execution_result) const {
        json my_ret_val = json::object();
        if (p_field_node->children.size() > 1) {
            const auto& myArgs = p_field_node->children[1];
            if (!myArgs || myArgs->type != "isched::v0_0_1::gql::Arguments") {
                p_execution_result.errors.push_back({EErrorCodes::PARSE_ERROR,
                    format("Expected arguments, got a {}", myArgs->type)});
                return my_ret_val;
            }
            for (const auto& myArg : myArgs->children) {
                if (myArg->type != "isched::v0_0_1::gql::Argument") {
                    p_execution_result.errors.push_back({EErrorCodes::PARSE_ERROR,
                        format("Expected an argument, got a {}", myArg->type)});
                    continue;
                }
                if (myArg->children.size() != 2) {
                    p_execution_result.errors.push_back({EErrorCodes::PARSE_ERROR, "Empty argument"});
                    continue;
                }
                const auto& myArgName = myArg->children[0]->string_view();
                my_ret_val[myArgName] = myArg->children[1]->string_view();
            }
        }
        return my_ret_val;
    }

    void GqlExecutor::process_field_selection(const TNodePtr &p_selection_set, ExecutionResult& p_result) const {
       for (const auto& mySelection : p_selection_set->children) {
           if (mySelection->type == "isched::v0_0_1::gql::SelectionSet") {
               process_field_selection(mySelection, p_result);
           } else if (mySelection->type == "isched::v0_0_1::gql::Selection") {
               if (mySelection->children.empty()) {
                   p_result.errors.push_back({EErrorCodes::PARSE_ERROR, "Empty selection"});
                   continue;
               }
               const auto& myField = mySelection->children[0];
               if (myField->type != "isched::v0_0_1::gql::Field") {
                   p_result.errors.push_back({EErrorCodes::PARSE_ERROR,
                       format("Expected a field, got a {}", myField->type)});
                   continue;
               }
               if (myField->children.empty()) {
                   p_result.errors.push_back({EErrorCodes::PARSE_ERROR, "Empty field"});
                   continue;
               }
               const auto& myFieldName = myField->children[0]->string_view();
               spdlog::debug("Checking resolver for field {} in Query type", myFieldName);
               if (!m_resolvers.has_resolver(std::string(myFieldName))) {
                   p_result.errors.push_back({
                       EErrorCodes::MISSING_GQL_RESOLVER,
                       std::format("Missing resolver for field {} in Query type", myFieldName)
                   });
               }
               if (p_result.data.empty()) {
                   p_result.data = json::object();
               }
               json my_args = process_arguments(myField,p_result);
               const ResolverFunction my_found_resolver = m_resolvers.get_resolver(std::string(myFieldName));
               json my_result = my_found_resolver(my_args,json::object());
               p_result.data["data"][myFieldName] = my_result;
           } else {
               p_result.errors.push_back({EErrorCodes::PARSE_ERROR,
                   format("Expected a selection or selection set, got a {}", mySelection->type)});
           }
       }
    }

    ExecutionResult GqlExecutor::load_schema(const std::string &pSchemaDocument) {
        static const std::string aName = "SchemaDocument";
        // Set up the states, here a single std::string as that is
        // what our action requires as an additional function argument.
        tao::pegtl::string_input in(std::move(pSchemaDocument), aName);
        ExecutionResult myResult;
        try {
            const auto myRetVal = gql::generate_ast_and_log<gql::Document>(in, aName);
            const auto& aRoot = std::move(std::get<1>(myRetVal));
            const bool aParsingOk = std::get<0>(myRetVal);
            if (!aParsingOk) {
                myResult.errors.push_back({EErrorCodes::PARSE_ERROR, "Failed to parse schema document"});
                return myResult;
            }
            if (aRoot && !aRoot->children.empty() && !aRoot->children[0]->children.empty()) {
                const auto& myTypedef = aRoot->children[0]->children[0];
                if (myTypedef->type != "isched::v0_0_1::gql::TypeDefinition") {
                    myResult.errors.push_back({
                        EErrorCodes::PARSE_ERROR,
                        "Expected with a type definition"
                    });
                }
                else {
                    const auto myTypedefName= myTypedef->children[0]->string_view();
                    if (myTypedefName == "Query") {
                        for (size_t myIdx=1; myIdx<myTypedef->children.size(); ++myIdx) {
                            process_field_definition(myResult, myTypedef, myIdx);
                        }
                    }
                }
            }
        } catch (const tao::pegtl::parse_error &e) {
            log_parse_error_exception(in, myResult, e);
        }
        return myResult;
    }

    void GqlExecutor::log_parse_error_exception(const tao::pegtl::string_input<> & in, ExecutionResult myResult, const tao::pegtl::parse_error &e) const {
        const auto p = e.positions().front();
        myResult.errors.push_back({
            EErrorCodes::PARSE_ERROR,
            std::format("Error parsing schema: message={}, line={} column={}.",
                        e.what(), in.line_at(p), p.column)
        });
    }

    ExecutionResult GqlExecutor::execute(const std::string_view p_query, const bool p_print_dot) const {
        static const std::string aName = "ExecutableDocument";
        // Set up the states, here a single std::string as that is
        // what our action requires as an additional function argument.
        tao::pegtl::string_input in(std::move(std::string(p_query)), aName);
        ExecutionResult myResult;
        try {
            const auto myRetVal = gql::generate_ast_and_log<gql::Document>(in, aName,false,p_print_dot);
            const auto& aRoot = std::move(std::get<1>(myRetVal));
            const bool aParsingOk = std::get<0>(myRetVal);
            if (!aParsingOk) {
                myResult.errors.push_back({EErrorCodes::PARSE_ERROR, "Failed to parse schema document"});
                return myResult;
            }
            if (!aRoot || aRoot->children.empty()) {
                myResult.errors.push_back({EErrorCodes::PARSE_ERROR, "Empty document"});
                return myResult;
            }
            const auto& myDoc = aRoot->children[0];
            if (!myDoc || myDoc->type != "isched::v0_0_1::gql::Document") {
                myResult.errors.push_back({EErrorCodes::PARSE_ERROR, "Expected with a document"});
                return myResult;
            }
            if (myDoc->children.empty()) {
                myResult.errors.push_back({EErrorCodes::PARSE_ERROR, "Empty document"});
                return myResult;
            }
            for (const auto& myChild : myDoc->children) {
                if (myChild->type != "isched::v0_0_1::gql::ExecutableDefinition") {
                    myResult.errors.push_back({EErrorCodes::PARSE_ERROR,
                        std::format("Expected with an executable definition, got {}.", myChild->type)});
                    return myResult;
                }
                for (const auto& myOperation : myChild->children) {
                    if (myOperation->type != "isched::v0_0_1::gql::OperationDefinition") {
                        myResult.errors.push_back({EErrorCodes::PARSE_ERROR,
                            std::format("Expected with an operation definition, got {}.", myOperation->type)});
                        return myResult;
                    }
                    if (myOperation->children.empty()) {
                        myResult.errors.push_back({EErrorCodes::PARSE_ERROR, "Empty operation definition"});
                        return myResult;
                    }
                    const auto a_op_type = myOperation->children[0]->string_view();
                    if (a_op_type=="query") {
                        for (size_t myIdx=1; myIdx<myOperation->children.size(); ++myIdx) {
                            if (myOperation->children[myIdx]->type == "isched::v0_0_1::gql::SelectionSet") {
                                process_field_selection(myOperation->children[myIdx], myResult);
                            }else {
                                myResult.errors.push_back({EErrorCodes::PARSE_ERROR,
                                    std::format("Expected with a selection set, got {}.", myOperation->children[myIdx]->type)});
                            }
                        }

                    } else {
                        myResult.errors.push_back({EErrorCodes::PARSE_ERROR,
                            std::format("Only query operations are supported, got {}.", a_op_type)});
                    }
                }
            }
        } catch (const tao::pegtl::parse_error &e) {
            log_parse_error_exception(in, myResult, e);
        }
        return myResult;
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
            log_parse_error_exception(in, {}, e);
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

    ExecutionResult GqlExecutor::execute(gql::Document p_doc) {
        return ExecutionResult{};
    };

    std::unique_ptr<GqlExecutor> GqlExecutor::create(std::shared_ptr<DatabaseManager> database) {
        return std::make_unique<GqlExecutor>(std::move(database));
    }
}
