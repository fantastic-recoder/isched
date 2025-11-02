/**
 * @file isched_graphql_executor.cpp
 * @brief Implementation of GraphQL query executor with smart pointer-based AST
 * @author Isched Development Team
 * @date 2024-12-20
 * @version 1.0.0
 */

#include "isched_graphql_executor.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <sstream>
#include <regex>
#include <chrono>

namespace isched::v0_0_1::backend {

// ============================================================================
// Field implementation
// ============================================================================

void Field::accept(const std::function<void(const ASTNode&)>& visitor) const {
    visitor(*this);
    for (const auto& arg : arguments) {
        if (arg) arg->accept(visitor);
    }
    for (const auto& selection : selection_set) {
        if (selection) selection->accept(visitor);
    }
}

// ============================================================================
// ExecutionResult implementation
// ============================================================================

nlohmann::json ExecutionResult::to_json() const {
    nlohmann::json result;
    
    if (!data.is_null()) {
        result["data"] = data;
    }
    
    if (!errors.empty()) {
        result["errors"] = errors;
    }
    
    if (!extensions.is_null()) {
        result["extensions"] = extensions;
    }
    
    return result;
}

// ============================================================================
// Document implementation
// ============================================================================

OperationPtr Document::get_operation(const std::string& operation_name) const {
    if (operations.empty()) {
        return nullptr;
    }
    
    // If no operation name specified, return the first operation
    if (operation_name.empty()) {
        return operations.front();
    }
    
    // Find operation by name
    for (const auto& operation : operations) {
        if (operation && operation->name == operation_name) {
            return operation;
        }
    }
    
    return nullptr;
}

// ============================================================================
// QueryComplexityAnalyzer implementation
// ============================================================================

QueryComplexityAnalyzer::ComplexityResult 
QueryComplexityAnalyzer::analyze(const DocumentPtr& document, 
                                 const std::string& operation_name) const {
    if (!document) {
        return {0, 0, 0, false, "Document is null"};
    }
    
    auto operation = document->get_operation(operation_name);
    if (!operation) {
        return {0, 0, 0, false, "Operation not found"};
    }
    
    return analyze_selections(operation->selection_set, 1);
}

QueryComplexityAnalyzer::ComplexityResult 
QueryComplexityAnalyzer::analyze_selections(const std::vector<SelectionPtr>& selections, 
                                           uint32_t depth) const {
    ComplexityResult result;
    result.depth = depth;
    
    if (depth > config_.max_depth) {
        result.is_valid = false;
        result.error_message = "Query depth exceeds maximum of " + std::to_string(config_.max_depth);
        return result;
    }
    
    for (const auto& selection : selections) {
        if (!selection) continue;
        
        result.field_count++;
        result.complexity += depth; // Simple complexity calculation
        
        // Check if this is a field selection with nested selections
        if (auto field_selection = std::dynamic_pointer_cast<FieldSelection>(selection)) {
            if (field_selection->field && !field_selection->field->selection_set.empty()) {
                auto nested_result = analyze_selections(field_selection->field->selection_set, depth + 1);
                if (!nested_result.is_valid) {
                    return nested_result;
                }
                result.depth = std::max(result.depth, nested_result.depth);
                result.complexity += nested_result.complexity;
                result.field_count += nested_result.field_count;
            }
        }
    }
    
    if (result.complexity > config_.max_complexity) {
        result.is_valid = false;
        result.error_message = "Query complexity exceeds maximum of " + std::to_string(config_.max_complexity);
    }
    
    if (result.field_count > config_.max_fields) {
        result.is_valid = false;
        result.error_message = "Query field count exceeds maximum of " + std::to_string(config_.max_fields);
    }
    
    return result;
}

// ============================================================================
// ResolverRegistry implementation
// ============================================================================

void ResolverRegistry::register_resolver(const std::string& field_name, ResolverFunction resolver) {
    resolvers_[field_name] = std::move(resolver);
}

nlohmann::json ResolverRegistry::resolve_field(
    const std::string& field_name,
    const nlohmann::json& parent,
    const std::vector<ArgumentPtr>& arguments,
    const nlohmann::json& context) const {
    
    auto it = resolvers_.find(field_name);
    if (it != resolvers_.end()) {
        return it->second(parent, arguments, context);
    }
    
    // Default resolver - try to extract field from parent object
    if (parent.is_object() && parent.contains(field_name)) {
        return parent[field_name];
    }
    
    return nlohmann::json();
}

bool ResolverRegistry::has_resolver(const std::string& field_name) const noexcept {
    return resolvers_.find(field_name) != resolvers_.end();
}

// ============================================================================
// GraphQLExecutor implementation
// ============================================================================

GraphQLExecutor::GraphQLExecutor(std::shared_ptr<DatabaseManager> database, Config config)
    : database_(std::move(database)), 
      config_(std::move(config)),
      complexity_analyzer_(config_.complexity) {
    setup_builtin_resolvers();
}

std::pair<DocumentPtr, std::vector<std::string>> GraphQLExecutor::parse(const std::string& query) const {
    if (query.length() > config_.max_query_length) {
        return {nullptr, {"Query length exceeds maximum allowed"}};
    }
    
    auto document = std::make_shared<Document>();
    std::vector<std::string> errors;
    
    // Simple regex-based parsing for basic queries
    // This is a minimal implementation for demonstration
    std::regex query_regex(R"(\s*\{\s*(\w+)\s*\}\s*)");
    std::smatch matches;
    
    if (std::regex_match(query, matches, query_regex)) {
        auto operation = std::make_shared<OperationDefinition>(OperationType::Query);
        
        std::string field_name = matches[1].str();
        auto field = std::make_shared<Field>(field_name);
        auto field_selection = std::make_shared<FieldSelection>(field);
        operation->selection_set.push_back(field_selection);
        
        document->operations.push_back(operation);
    } else {
        // Handle more complex queries with nested structure
        std::regex complex_query_regex(R"(\s*\{\s*(\w+)\s*\{\s*(\w+)\s*\}\s*\}\s*)");
        if (std::regex_match(query, matches, complex_query_regex)) {
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
            errors.push_back("Failed to parse query: unsupported syntax");
        }
    }
    
    return {document, errors};
}

ExecutionResult GraphQLExecutor::execute(const std::string& query,
                                       const nlohmann::json& variables,
                                       const std::string& operation_name,
                                       const nlohmann::json& context) const {
    auto start_time = std::chrono::high_resolution_clock::now();
    ExecutionResult result;
    
    try {
        // Parse query
        auto [document, parse_errors] = parse(query);
        if (!parse_errors.empty()) {
            result.errors = parse_errors;
            return result;
        }
        
        if (!document) {
            result.errors.push_back("Failed to parse document");
            return result;
        }
        
        // Analyze complexity
        auto complexity_result = complexity_analyzer_.analyze(document, operation_name);
        if (!complexity_result.is_valid) {
            result.errors.push_back("Query complexity error: " + complexity_result.error_message);
            return result;
        }
        
        // Find operation to execute
        auto operation = document->get_operation(operation_name);
        if (!operation) {
            result.errors.push_back("Operation not found: " + operation_name);
            return result;
        }
        
        // Execute operation
        result.data = execute_operation(operation, variables, context);
        
    } catch (const std::exception& e) {
        result.errors.push_back(std::string("Execution error: ") + e.what());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    return result;
}

nlohmann::json GraphQLExecutor::execute_operation(const OperationPtr& operation,
                                                 const nlohmann::json& variables,
                                                 const nlohmann::json& context) const {
    if (!operation) {
        return nlohmann::json();
    }
    
    // For queries, execute with empty root object
    nlohmann::json root_object = nlohmann::json::object();
    
    return execute_selections(operation->selection_set, root_object, variables, context);
}

nlohmann::json GraphQLExecutor::execute_selections(const std::vector<SelectionPtr>& selections,
                                                  const nlohmann::json& parent,
                                                  const nlohmann::json& variables,
                                                  const nlohmann::json& context) const {
    nlohmann::json result = nlohmann::json::object();
    
    for (const auto& selection : selections) {
        if (!selection) continue;
        
        if (auto field_selection = std::dynamic_pointer_cast<FieldSelection>(selection)) {
            if (field_selection->field) {
                auto field_result = execute_field(field_selection->field, parent, variables, context);
                result[field_selection->field->get_response_key()] = field_result;
            }
        }
    }
    
    return result;
}

nlohmann::json GraphQLExecutor::execute_field(const FieldPtr& field,
                                             const nlohmann::json& parent,
                                             const nlohmann::json& variables,
                                             const nlohmann::json& context) const {
    if (!field) {
        return nlohmann::json();
    }
    
    // Resolve field value using registered resolver
    auto field_value = resolver_registry_.resolve_field(field->name, parent, field->arguments, context);
    
    // If field has nested selections, execute them
    if (!field->selection_set.empty()) {
        if (field_value.is_object()) {
            return execute_selections(field->selection_set, field_value, variables, context);
        } else if (field_value.is_array()) {
            nlohmann::json array_result = nlohmann::json::array();
            for (const auto& item : field_value) {
                if (item.is_object()) {
                    array_result.push_back(execute_selections(field->selection_set, item, variables, context));
                } else {
                    array_result.push_back(item);
                }
            }
            return array_result;
        }
    }
    
    return field_value;
}

void GraphQLExecutor::setup_builtin_resolvers() {
    // Hello resolver
    resolver_registry_.register_resolver("hello", [](const nlohmann::json&, const std::vector<ArgumentPtr>&, const nlohmann::json&) {
        return nlohmann::json("Hello, GraphQL!");
    });
    
    // Version resolver
    resolver_registry_.register_resolver("version", [](const nlohmann::json&, const std::vector<ArgumentPtr>&, const nlohmann::json&) {
        return nlohmann::json("1.0.0");
    });
    
    // Uptime resolver
    resolver_registry_.register_resolver("uptime", [](const nlohmann::json&, const std::vector<ArgumentPtr>&, const nlohmann::json&) {
        static auto start_time = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        return nlohmann::json(uptime_seconds);
    });
    
    // Client count resolver (placeholder)
    resolver_registry_.register_resolver("clientCount", [](const nlohmann::json&, const std::vector<ArgumentPtr>&, const nlohmann::json&) {
        return nlohmann::json(1); // Placeholder value
    });
    
    // Schema introspection resolver
    resolver_registry_.register_resolver("__schema", [](const nlohmann::json&, const std::vector<ArgumentPtr>&, const nlohmann::json&) {
        nlohmann::json schema;
        schema["queryType"] = {{"name", "Query"}};
        schema["mutationType"] = nullptr;
        schema["subscriptionType"] = nullptr;
        schema["types"] = nlohmann::json::array();
        return schema;
    });
}

} // namespace isched::v0_0_1::backend

#include "isched_graphql_executor.hpp"
#include <spdlog/spdlog.h>
#include <tao/pegtl/contrib/parse_tree.hpp>
#include <algorithm>
#include <sstream>
#include <regex>

namespace isched::v0_0_1::backend {

// ============================================================================
// Field implementation
// ============================================================================

void Field::accept(const std::function<void(const ASTNode&)>& visitor) const {
    visitor(*this);
    for (const auto& arg : arguments) {
        if (arg) arg->accept(visitor);
    }
    for (const auto& selection : selection_set) {
        if (selection) selection->accept(visitor);
    }
}

namespace isched::v0_0_1::backend {

// ============================================================================
// ExecutionResult Implementation
// ============================================================================

nlohmann::json ExecutionResult::to_json() const {
    nlohmann::json result;
    
    if (!data.is_null()) {
        result["data"] = data;
    }
    
    if (!errors.empty()) {
        result["errors"] = errors;
    }
    
    if (!extensions.is_null()) {
        result["extensions"] = extensions;
    }
    
    return result;
}

// ============================================================================
// Document Implementation
// ============================================================================

OperationPtr Document::get_operation(const std::string& operation_name) const {
    if (operations.empty()) {
        return nullptr;
    }
    
    // If no operation name specified, return the first (and hopefully only) operation
    if (operation_name.empty()) {
        return operations.front();
    }
    
    // Find operation by name
    for (const auto& operation : operations) {
        if (operation && operation->name == operation_name) {
            return operation;
        }
    }
    
    return nullptr;
}

// ============================================================================
// QueryComplexityAnalyzer Implementation
// ============================================================================

QueryComplexityAnalyzer::ComplexityResult QueryComplexityAnalyzer::analyze(
    const DocumentPtr& document, const std::string& operation_name) const {
    
    if (!document) {
        return {0, 0, 0, false, "Invalid document"};
    }
    
    auto operation = document->get_operation(operation_name);
    if (!operation) {
        return {0, 0, 0, false, "Operation not found"};
    }
    
    return analyze_selections(operation->selection_set, 0);
}

QueryComplexityAnalyzer::ComplexityResult QueryComplexityAnalyzer::analyze_selections(
    const std::vector<SelectionPtr>& selections, uint32_t depth) const {
    
    ComplexityResult result;
    result.depth = depth;
    
    if (depth > config_.max_depth) {
        result.is_valid = false;
        result.error_message = "Query depth exceeds maximum allowed (" + std::to_string(config_.max_depth) + ")";
        return result;
    }
    
    for (const auto& selection : selections) {
        if (!selection) continue;
        
        // For now, we only handle field selections
        auto field_selection = std::dynamic_pointer_cast<FieldSelection>(selection);
        if (!field_selection || !field_selection->field) continue;
        
        result.field_count++;
        result.complexity += (depth + 1);  // Simple complexity calculation
        
        // Recursively analyze nested selections
        if (!field_selection->field->selection_set.empty()) {
            auto nested_result = analyze_selections(field_selection->field->selection_set, depth + 1);
            
            if (!nested_result.is_valid) {
                return nested_result;
            }
            
            result.depth = std::max(result.depth, nested_result.depth);
            result.complexity += nested_result.complexity;
            result.field_count += nested_result.field_count;
        }
    }
    
    if (result.complexity > config_.max_complexity) {
        result.is_valid = false;
        result.error_message = "Query complexity exceeds maximum allowed (" + std::to_string(config_.max_complexity) + ")";
        return result;
    }
    
    if (result.field_count > config_.max_fields) {
        result.is_valid = false;
        result.error_message = "Query field count exceeds maximum allowed (" + std::to_string(config_.max_fields) + ")";
        return result;
    }
    
    return result;
}

// ============================================================================
// ResolverRegistry Implementation
// ============================================================================

void ResolverRegistry::register_resolver(const std::string& type_name,
                                        const std::string& field_name,
                                        ResolverFunction resolver) {
    resolvers_[type_name][field_name] = std::move(resolver);
}

ResolverFunction ResolverRegistry::get_resolver(const std::string& type_name,
                                               const std::string& field_name) const {
    auto type_it = resolvers_.find(type_name);
    if (type_it == resolvers_.end()) {
        return nullptr;
    }
    
    auto field_it = type_it->second.find(field_name);
    if (field_it == type_it->second.end()) {
        return nullptr;
    }
    
    return field_it->second;
}

bool ResolverRegistry::has_resolver(const std::string& type_name,
                                   const std::string& field_name) const {
    auto type_it = resolvers_.find(type_name);
    if (type_it == resolvers_.end()) {
        return false;
    }
    
    return type_it->second.find(field_name) != type_it->second.end();
}

// ============================================================================
// PEGTL Grammar Actions for AST Building
// ============================================================================

namespace grammar {
    
    // Actions for building AST during parsing
    template<typename Rule>
    struct action {};
    
    // Helper function to extract string content from PEGTL input
    template<typename Input>
    std::string extract_string(const Input& input) {
        return std::string(input.begin(), input.end());
    }
    
    // Helper function to create GraphQL value from string
    GraphQLValue parse_value(const std::string& value_str, const std::string& type) {
        if (type == "string") {
            // Remove quotes and handle escape sequences
            if (value_str.size() >= 2 && value_str.front() == '"' && value_str.back() == '"') {
                std::string unquoted = value_str.substr(1, value_str.size() - 2);
                // Simple escape handling (full implementation would handle all GraphQL escapes)
                std::regex escape_regex(R"(\\(.))");
                return std::regex_replace(unquoted, escape_regex, "$1");
            }
            return value_str;
        } else if (type == "int") {
            return static_cast<int64_t>(std::stoll(value_str));
        } else if (type == "float") {
            return std::stod(value_str);
        } else if (type == "boolean") {
            return value_str == "true";
        } else if (type == "null") {
            return nullptr;
        }
        
        return value_str; // fallback
    }
    
    // Simple parser state for building AST
    struct SimpleParseState {
        DocumentPtr document;
        std::vector<OperationPtr> operation_stack;
        std::vector<FieldPtr> field_stack;
        std::vector<std::string> errors;
        
        SimpleParseState() : document(std::make_shared<::isched::v0_0_1::backend::Document>()) {}
        
        void add_error(const std::string& error) {
            errors.push_back(error);
        }
    };
    
    // Parse a simple GraphQL query using string parsing
    // This is a simplified parser that handles basic queries
    DocumentPtr parse_simple_query(const std::string& query) {
        auto document = std::make_shared<Document>();
        
        // Simple regex-based parsing for basic queries
        // This is a minimal implementation for demonstration
        std::regex query_regex(R"(\s*\{\s*(\w+)\s*\}\s*)");
        std::smatch matches;
        
        if (std::regex_match(query, matches, query_regex)) {
            auto operation = std::make_shared<OperationDefinition>(::isched::v0_0_1::backend::OperationType::Query);
            
            std::string field_name = matches[1].str();
            auto field = std::make_shared<::isched::v0_0_1::backend::Field>(field_name);
            auto field_selection = std::make_shared<::isched::v0_0_1::backend::FieldSelection>(field);
            operation->selection_set.push_back(field_selection);
            
            document->operations.push_back(operation);
        } else {
            // Handle more complex queries with nested structure
            std::regex complex_query_regex(R"(\s*\{\s*(\w+)\s*\{\s*(\w+)\s*\}\s*\}\s*)");
            if (std::regex_match(query, matches, complex_query_regex)) {
                auto operation = std::make_shared<OperationDefinition>(::isched::v0_0_1::backend::OperationType::Query);
                
                std::string parent_field = matches[1].str();
                std::string nested_field = matches[2].str();
                
                auto parent_field_obj = std::make_shared<::isched::v0_0_1::backend::Field>(parent_field);
                auto nested_field_obj = std::make_shared<::isched::v0_0_1::backend::Field>(nested_field);
                auto nested_selection = std::make_shared<::isched::v0_0_1::backend::FieldSelection>(nested_field_obj);
                
                parent_field_obj->selection_set.push_back(nested_selection);
                auto parent_selection = std::make_shared<::isched::v0_0_1::backend::FieldSelection>(parent_field_obj);
                operation->selection_set.push_back(parent_selection);
                
                document->operations.push_back(operation);
            }
        }
        
        return document;
    }
    
} // namespace grammar

// ============================================================================
// GraphQLExecutor Implementation
// ============================================================================

GraphQLExecutor::GraphQLExecutor(std::shared_ptr<DatabaseManager> database, Config config)
    : database_(std::move(database))
    , config_(config)
    , complexity_analyzer_(config.complexity) {
    
    setup_builtin_resolvers();
    
    spdlog::info("GraphQL executor initialized with complexity limits: "
                "max_depth={}, max_complexity={}, max_fields={}",
                config_.complexity.max_depth,
                config_.complexity.max_complexity,
                config_.complexity.max_fields);
}

std::pair<DocumentPtr, std::vector<std::string>> GraphQLExecutor::parse(const std::string& query) const {
    std::vector<std::string> errors;
    
    // Validate query length
    if (query.length() > config_.max_query_length) {
        errors.push_back("Query exceeds maximum length of " + std::to_string(config_.max_query_length) + " characters");
        return {nullptr, errors};
    }
    
    // Basic query validation
    if (query.empty()) {
        errors.push_back("Empty query string");
        return {nullptr, errors};
    }
    
    try {
        // Use simplified parser for now
        auto document = grammar::parse_simple_query(query);
        
        if (!document || document->operations.empty()) {
            errors.push_back("Failed to parse GraphQL query - invalid syntax");
            return {nullptr, errors};
        }
        
        return {document, errors};
        
    } catch (const std::exception& e) {
        errors.push_back("Parse error: " + std::string(e.what()));
        return {nullptr, errors};
    }
}

ExecutionResult GraphQLExecutor::execute(const std::string& query,
                                        const nlohmann::json& variables,
                                        const std::string& operation_name,
                                        const nlohmann::json& context) const {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    ExecutionResult result;
    
    // Parse the query
    auto [document, parse_errors] = parse(query);
    
    if (!document) {
        for (const auto& error : parse_errors) {
            result.errors.push_back({{"message", error}});
        }
        auto end_time = std::chrono::high_resolution_clock::now();
        result.execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        return result;
    }
    
    return execute_document(document, variables, operation_name, context);
}

ExecutionResult GraphQLExecutor::execute_document(const DocumentPtr& document,
                                                 const nlohmann::json& variables,
                                                 const std::string& operation_name,
                                                 const nlohmann::json& context) const {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    ExecutionResult result;
    
    try {
        // Get the operation to execute
        auto operation = document->get_operation(operation_name);
        if (!operation) {
            result.errors.push_back({{"message", "Operation not found: " + operation_name}});
            auto end_time = std::chrono::high_resolution_clock::now();
            result.execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            return result;
        }
        
        // Analyze query complexity
        auto complexity_result = complexity_analyzer_.analyze(document, operation_name);
        if (!complexity_result.is_valid) {
            result.errors.push_back({{"message", complexity_result.error_message}});
            auto end_time = std::chrono::high_resolution_clock::now();
            result.execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            return result;
        }
        
        // Add complexity info to extensions
        result.extensions = {
            {"complexity", {
                {"depth", complexity_result.depth},
                {"complexity", complexity_result.complexity},
                {"fieldCount", complexity_result.field_count}
            }}
        };
        
        // Execute the operation
        result.data = execute_operation(operation, variables, context);
        
    } catch (const std::exception& e) {
        result.errors.push_back({{"message", "Execution error: " + std::string(e.what())}});
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    spdlog::debug("GraphQL query executed in {} microseconds", result.execution_time.count());
    
    return result;
}

nlohmann::json GraphQLExecutor::execute_operation(const OperationPtr& operation,
                                                  const nlohmann::json& variables,
                                                  const nlohmann::json& context) const {
    
    // For queries, start with a null root and execute selection set
    nlohmann::json root_value = nullptr;
    std::string root_type = "Query"; // Default root type
    
    switch (operation->operation_type) {
        case OperationType::Query:
            root_type = "Query";
            break;
        case OperationType::Mutation:
            root_type = "Mutation";
            break;
        case OperationType::Subscription:
            root_type = "Subscription";
            break;
    }
    
    return execute_selection_set(operation->selection_set, root_value, root_type, context);
}

nlohmann::json GraphQLExecutor::execute_selection_set(
    const std::vector<SelectionPtr>& selections,
    const nlohmann::json& parent_value,
    const std::string& parent_type,
    const nlohmann::json& context) const {
    
    nlohmann::json result = nlohmann::json::object();
    
    for (const auto& selection : selections) {
        if (!selection) continue;
        
        // Handle field selections
        auto field_selection = std::dynamic_pointer_cast<FieldSelection>(selection);
        if (field_selection && field_selection->field) {
            auto field = field_selection->field;
            auto field_result = execute_field(field, parent_value, parent_type, context);
            
            // Use alias if present, otherwise use field name
            std::string response_key = field->get_response_key();
            result[response_key] = field_result;
        }
    }
    
    return result;
}

nlohmann::json GraphQLExecutor::execute_field(const FieldPtr& field,
                                              const nlohmann::json& parent_value,
                                              const std::string& parent_type,
                                              const nlohmann::json& context) const {
    
    // Look for a resolver for this field
    auto resolver = resolvers_.get_resolver(parent_type, field->name);
    if (resolver) {
        auto field_value = resolver(field, parent_value, context);
        
        // If the field has a selection set, execute it on the resolved value
        if (!field->selection_set.empty() && !field_value.is_null()) {
            // For simplicity, assume the field type is the field name capitalized
            std::string field_type = field->name;
            field_type[0] = std::toupper(field_type[0]);
            
            return execute_selection_set(field->selection_set, field_value, field_type, context);
        }
        
        return field_value;
    }
    
    // Default behavior: return null for unknown fields
    spdlog::warn("No resolver found for field '{}' on type '{}'", field->name, parent_type);
    return nullptr;
}

void GraphQLExecutor::setup_builtin_resolvers() {
    // Query.hello resolver
    resolvers_.register_resolver("Query", "hello", [](const FieldPtr& field, 
                                                     const nlohmann::json& parent, 
                                                     const nlohmann::json& context) -> nlohmann::json {
        (void)parent; // Suppress unused parameter warning
        (void)context;
        
        // Check for name argument
        for (const auto& arg : field->arguments) {
            if (arg && arg->name == "name") {
                if (std::holds_alternative<std::string>(arg->value)) {
                    std::string name = std::get<std::string>(arg->value);
                    return "Hello, " + name + "!";
                }
            }
        }
        
        return "Hello, World!";
    });
    
    // Query.version resolver
    resolvers_.register_resolver("Query", "version", [](const FieldPtr& field,
                                                       const nlohmann::json& parent,
                                                       const nlohmann::json& context) -> nlohmann::json {
        (void)field;
        (void)parent;
        (void)context;
        
        return "1.0.0";
    });
    
    // Query.uptime resolver
    resolvers_.register_resolver("Query", "uptime", [](const FieldPtr& field,
                                                      const nlohmann::json& parent,
                                                      const nlohmann::json& context) -> nlohmann::json {
        (void)field;
        (void)parent;
        (void)context;
        
        // Simple uptime calculation (this could be enhanced with actual server start time)
        static auto start_time = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        
        return uptime_seconds;
    });
    
    // Query.clientCount resolver (placeholder)
    resolvers_.register_resolver("Query", "clientCount", [](const FieldPtr& field,
                                                           const nlohmann::json& parent,
                                                           const nlohmann::json& context) -> nlohmann::json {
        (void)field;
        (void)parent;
        (void)context;
        
        // This would be connected to actual tenant manager in a full implementation
        return 0;
    });
    
    // Query.__schema resolver for introspection
    resolvers_.register_resolver("Query", "__schema", [](const FieldPtr& field,
                                                        const nlohmann::json& parent,
                                                        const nlohmann::json& context) -> nlohmann::json {
        (void)field;
        (void)parent;
        (void)context;
        
        // Basic schema introspection
        return nlohmann::json{
            {"types", nlohmann::json::array({
                {
                    {"name", "Query"},
                    {"kind", "OBJECT"},
                    {"fields", nlohmann::json::array({
                        {{"name", "hello"}, {"type", {{"name", "String"}}}},
                        {{"name", "version"}, {"type", {{"name", "String"}}}},
                        {{"name", "uptime"}, {"type", {{"name", "Int"}}}},
                        {{"name", "clientCount"}, {"type", {{"name", "Int"}}}}
                    })}
                }
            })}
        };
    });
    
    spdlog::info("Built-in GraphQL resolvers registered");
}

} // namespace isched::v0_0_1::backend