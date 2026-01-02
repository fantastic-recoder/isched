//
// Created by groby on 2025-12-19.
//

#include "isched_GqlExecutor.hpp"

#include <regex>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <tao/pegtl.hpp>

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
        if (myField->type != "isched::v0_0_1::gql::FieldDefinition") {
            return;
        }
        if (myField->children.empty() ) {
            p_result.errors.push_back(ExecutionError{
                EErrorCodes::PARSE_ERROR,"Incomplete Query field definition"});
        } else {
            const auto myFieldName = myField->children[0]->string_view();
            spdlog::debug("Checking resolver for field {} in Query type", myFieldName);
            if (!m_resolvers.has_resolver(std::string(myFieldName))) {
                p_result.errors.push_back(ExecutionError{
                    EErrorCodes::MISSING_GQL_RESOLVER,
                    std::format("Missing resolver for field {} in Query type", myFieldName)
                });
            }
        }
    }

    json GqlExecutor::extract_argument_value(const TNodePtr &p_arg, ExecutionResult& p_execution_result) const {
        json my_ret_val;
        if (p_arg->type == "isched::v0_0_1::gql::StringValue") {
            const std::string my_ret_val_str(p_arg->string_view());
            if (my_ret_val_str.starts_with("\"\"\"")) {
                 my_ret_val = my_ret_val_str.substr(3, my_ret_val_str.length() - 6);
            } else if (my_ret_val_str.length() >= 2 && my_ret_val_str.front() == '"') {
                my_ret_val = my_ret_val_str.substr(1, my_ret_val_str.length()-2);
            } else { my_ret_val = my_ret_val_str; }
        } else if (p_arg->type == "isched::v0_0_1::gql::IntValue") {
            const std::string_view my_ret_val_str(p_arg->string_view());
            my_ret_val = std::stoll(std::string(my_ret_val_str));
        } else if (p_arg->type == "isched::v0_0_1::gql::FloatValue") {
            const std::string_view my_ret_val_str(p_arg->string_view());
            my_ret_val = std::stod(std::string(my_ret_val_str));
        } else if (p_arg->type == "isched::v0_0_1::gql::BooleanValue") {
            const std::string_view my_ret_val_str(p_arg->string_view());
            my_ret_val = (my_ret_val_str == "true");
        } else if (p_arg->type == "isched::v0_0_1::gql::NullValue") {
            my_ret_val = nullptr;
        } else if (p_arg->type == "isched::v0_0_1::gql::EnumValue") {
            my_ret_val = std::string(p_arg->string_view());
        } else if (p_arg->type == "isched::v0_0_1::gql::ListValue") {
            my_ret_val = json::array();
            for (const auto& myChild : p_arg->children) {
                my_ret_val.push_back(extract_argument_value(myChild, p_execution_result));
            }
        } else if (p_arg->type == "isched::v0_0_1::gql::ObjectValue") {
            my_ret_val = json::object();
            for (const auto& myField : p_arg->children) {
                if (myField->type == "isched::v0_0_1::gql::ObjectField") {
                    const auto myFieldName = std::string(myField->children[0]->string_view());
                    my_ret_val[myFieldName] = extract_argument_value(myField->children[1], p_execution_result);
                }
            }
        } else if (p_arg->type == "isched::v0_0_1::gql::Value") {
             if (!p_arg->children.empty()) {
                 return extract_argument_value(p_arg->children[0], p_execution_result);
             }
        } else if (p_arg->type == "isched::v0_0_1::gql::Argument") {
             if (p_arg->children.size() > 1) {
                 return extract_argument_value(p_arg->children[1], p_execution_result);
             }
        } else {
            p_execution_result.errors.push_back(ExecutionError{EErrorCodes::PARSE_ERROR,
                format("Unknown argument value type: {}.", p_arg->type)});
        }
        return my_ret_val;
    }

    json GqlExecutor::process_arguments(const TNodePtr &p_field_node, ExecutionResult &p_execution_result) const {
        json my_ret_val = json::object();
        if (p_field_node->children.size() > 1) {
            const auto& myArgs = p_field_node->children[1];
            if (!myArgs || myArgs->type != "isched::v0_0_1::gql::Arguments") {
                p_execution_result.errors.push_back(ExecutionError{.code=EErrorCodes::PARSE_ERROR,
                    .message=format("Expected arguments, got a {}", myArgs->type)});
                return my_ret_val;
            }
            for (const auto& myArg : myArgs->children) {
                if (myArg->type != "isched::v0_0_1::gql::Argument") {
                    p_execution_result.errors.push_back(ExecutionError{.code=EErrorCodes::PARSE_ERROR,
                        .message=format("Expected an argument, got a {}", myArg->type)});
                    continue;
                }
                if (myArg->children.size() != 2) {
                    p_execution_result.errors.push_back(ExecutionError{.code=EErrorCodes::PARSE_ERROR, .message="Empty argument"});
                    continue;
                }
                const auto myArgName = std::string(myArg->children[0]->string_view());
                my_ret_val[myArgName] = extract_argument_value(myArg->children[1], p_execution_result);
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
                   p_result.errors.push_back(ExecutionError{.code=EErrorCodes::PARSE_ERROR, .message="Empty selection"});
                   continue;
               }
               const auto& myField = mySelection->children[0];
               if (myField->type != "isched::v0_0_1::gql::Field") {
                   p_result.errors.push_back(ExecutionError{.code=EErrorCodes::PARSE_ERROR,
                       .message=format("Expected a field, got a {}", myField->type)});
                   continue;
               }
               if (myField->children.empty()) {
                   p_result.errors.push_back(ExecutionError{.code=EErrorCodes::PARSE_ERROR, .message="Empty field"});
                   continue;
               }
               const auto& myFieldName = myField->children[0]->string_view();
               spdlog::debug("Checking resolver for field {} in Query type", myFieldName);
               if (!m_resolvers.has_resolver(std::string(myFieldName))) {
                   p_result.errors.push_back(ExecutionError{
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
               p_result.data[myFieldName] = my_result;
           } else {
               p_result.errors.push_back(ExecutionError{EErrorCodes::PARSE_ERROR,
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
            auto myRetVal = gql::generate_ast_and_log<gql::Document>(in, aName);
            const bool aParsingOk = std::get<0>(myRetVal);
            auto aRoot=std::get<1>(std::move(myRetVal));
            if (!aParsingOk) {
                myResult.errors.push_back(ExecutionError{EErrorCodes::PARSE_ERROR, "Failed to parse schema document"});
                return myResult;
            }
            m_current_schema=std::move(aRoot);
            if (m_current_schema && !m_current_schema->children.empty()) {
                const TNodePtr& myDoc = (m_current_schema->type == "isched::v0_0_1::gql::Document") ? m_current_schema : m_current_schema->children[0];
                
                for (const auto& myDefNode : myDoc->children) {
                    if (myDefNode->type == "isched::v0_0_1::gql::TypeDefinition" || myDefNode->type == "isched::v0_0_1::gql::ObjectTypeDefinition") {
                        const auto myTypedefName= myDefNode->children[0]->string_view();
                        if (myTypedefName == "Query") {
                            for (size_t myIdx=1; myIdx<myDefNode->children.size(); ++myIdx) {
                                 process_field_definition(myResult, myDefNode, myIdx);
                            }
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
        myResult.errors.push_back(ExecutionError{
            EErrorCodes::PARSE_ERROR,
            std::format("Error parsing schema: message={}, line={} column={}.",
                        e.what(), in.line_at(p), p.column)
        });
    }

    bool GqlExecutor::process_operation_definitions(ExecutionResult& p_result, const TNodePtr &myOperation) const {
        if (myOperation->type != "isched::v0_0_1::gql::OperationDefinition") {
            p_result.errors.push_back(ExecutionError{EErrorCodes::PARSE_ERROR,
                std::format("Expected with an operation definition, got {}.", myOperation->type)});
            return true;
        }
        if (myOperation->children.empty()) {
            p_result.errors.push_back(ExecutionError{EErrorCodes::PARSE_ERROR, "Empty operation definition"});
            return true;
        }
        const auto a_op_type = myOperation->children[0]->string_view();
        if (a_op_type=="query") {
            for (size_t myIdx=1; myIdx<myOperation->children.size(); ++myIdx) {
                if (myOperation->children[myIdx]->type == "isched::v0_0_1::gql::SelectionSet") {
                    process_field_selection(myOperation->children[myIdx], p_result);
                }else {
                    p_result.errors.push_back(ExecutionError{EErrorCodes::PARSE_ERROR,
                        std::format("Expected with a selection set, got {}.", myOperation->children[myIdx]->type)});
                }
            }

        } else if (myOperation->children[0]->type == "isched::v0_0_1::gql::SelectionSet") {
            process_field_selection(myOperation->children[0], p_result);
        } else {
            p_result.errors.push_back(ExecutionError{EErrorCodes::PARSE_ERROR,
                std::format("Only query operations are supported, got {}.", a_op_type)});
        }
        return false;
    }

    ExecutionResult GqlExecutor::execute(const std::string_view p_query, const bool p_print_dot) const {
        static const std::string aName = "ExecutableDocument";
        // Set up the states, here a single std::string as that is
        // what our action requires as an additional function argument.
        tao::pegtl::string_input in(std::move(std::string(p_query)), aName);
        ExecutionResult myResult;
        try {
            auto myRetVal = gql::generate_ast_and_log<gql::Document>(in, aName,false,p_print_dot);
            auto aRoot = std::get<1>(std::move(myRetVal));
            const bool aParsingOk = std::get<0>(myRetVal);
            if (!aParsingOk) {
                myResult.errors.push_back(ExecutionError{EErrorCodes::PARSE_ERROR, "Failed to parse schema document"});
                return myResult;
            }
            if (!aRoot || aRoot->children.empty()) {
                myResult.errors.push_back(ExecutionError{EErrorCodes::PARSE_ERROR, "Empty document"});
                return myResult;
            }
            const auto& myDoc = aRoot->children[0];
            if (!myDoc || myDoc->type != "isched::v0_0_1::gql::Document") {
                myResult.errors.push_back(ExecutionError{EErrorCodes::PARSE_ERROR, "Expected with a document"});
                return myResult;
            }
            if (myDoc->children.empty()) {
                myResult.errors.push_back(ExecutionError{EErrorCodes::PARSE_ERROR, "Empty document"});
                return myResult;
            }
            for (const auto& myChild : myDoc->children) {
                if (myChild->type != "isched::v0_0_1::gql::ExecutableDefinition") {
                    myResult.errors.push_back(ExecutionError{EErrorCodes::PARSE_ERROR,
                        std::format("Expected with an executable definition, got {}.", myChild->type)});
                    return myResult;
                }
                for (const auto& myOperation : myChild->children) {
                    process_operation_definitions(myResult, myOperation);
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
