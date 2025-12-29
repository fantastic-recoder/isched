/**
 * @file isched_GraphQLExecutor.hpp
 * @brief Simple GraphQL query executor with smart pointer-based AST
 * @author Isched Development Team
 * @date 2024-12-20
 * @version 1.0.0
 */

#pragma once

#include "isched_common.hpp"
#include "isched_DatabaseManager.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <regex>

namespace isched::v0_0_1::backend {

// Forward declarations
class GraphQLExecutor;
class ASTNode;
class Field;
class Selection;
class Document;
class OperationDefinition;

// Smart pointer type aliases for better readability
using ASTNodePtr = std::shared_ptr<ASTNode>;
using FieldPtr = std::shared_ptr<Field>;
using SelectionPtr = std::shared_ptr<Selection>;
using DocumentPtr = std::shared_ptr<Document>;
using OperationPtr = std::shared_ptr<OperationDefinition>;

/**
 * @brief GraphQL operation types
 */
enum class OperationType {
    Query,
    Mutation, 
    Subscription
};

/**
 * @brief Base class for all AST nodes in the GraphQL query tree
 */
class ASTNode {
public:
    virtual ~ASTNode() = default;
    
    /**
     * @brief Get the type name of this AST node
     * @return String representation of the node type
     */
    [[nodiscard]] virtual std::string get_type() const = 0;
    
    /**
     * @brief Accept visitor pattern for AST traversal
     * @param visitor Function to call for this node and its children
     */
    virtual void accept(const std::function<void(const ASTNode&)>& visitor) const = 0;

protected:
    ASTNode() = default;
};

/**
 * @brief Base class for GraphQL selections (fields, inline fragments, etc.)
 */
class Selection : public ASTNode {
public:
    ~Selection() override = default;
    
protected:
    Selection() = default;
};

/**
 * @brief GraphQL field with arguments and selections
 */
class Field final : public ASTNode {
public:
    std::string name;                                 ///< Field name
    std::string alias;                                ///< Optional field alias
    std::vector<SelectionPtr> selection_set;          ///< Nested field selections
    
    explicit Field(std::string field_name) : name(std::move(field_name)) {}
    
    [[nodiscard]] std::string get_type() const override { return "Field"; }
    
    void accept(const std::function<void(const ASTNode&)>& visitor) const override {
        visitor(*this);
        for (const auto& selection : selection_set) {
            if (selection) selection->accept(visitor);
        }
    }
    
    /**
     * @brief Get effective field name (alias if present, otherwise name)
     * @return The name to use in response
     */
    [[nodiscard]] const std::string& get_response_key() const noexcept {
        return alias.empty() ? name : alias;
    }
};

/**
 * @brief Field selection implementation
 */
class FieldSelection final : public Selection {
public:
    FieldPtr field;
    
    explicit FieldSelection(FieldPtr field_ptr) : field(std::move(field_ptr)) {}
    
    [[nodiscard]] std::string get_type() const override { return "FieldSelection"; }
    
    void accept(const std::function<void(const ASTNode&)>& visitor) const override {
        visitor(*this);
        if (field) field->accept(visitor);
    }
};

/**
 * @brief GraphQL operation definition (query, mutation, subscription)
 */
class OperationDefinition final : public ASTNode {
public:
    OperationType operation_type;                     ///< Type of operation
    std::string name;                                 ///< Optional operation name
    std::vector<SelectionPtr> selection_set;          ///< Root field selections
    
    explicit OperationDefinition(OperationType type) : operation_type(type) {}
    
    [[nodiscard]] std::string get_type() const override { return "OperationDefinition"; }
    
    void accept(const std::function<void(const ASTNode&)>& visitor) const override {
        visitor(*this);
        for (const auto& selection : selection_set) {
            if (selection) selection->accept(visitor);
        }
    }
};

/**
 * @brief Root GraphQL document containing operations
 */
class Document final : public ASTNode {
public:
    std::vector<OperationPtr> operations;             ///< Document operations
    
    Document() = default;
    
    [[nodiscard]] std::string get_type() const override { return "Document"; }
    
    void accept(const std::function<void(const ASTNode&)>& visitor) const override {
        visitor(*this);
        for (const auto& operation : operations) {
            if (operation) operation->accept(visitor);
        }
    }
    
    /**
     * @brief Get the operation to execute (default if unnamed query exists)
     * @param operation_name Name of operation to find (empty for default)
     * @return Pointer to operation or nullptr if not found
     */
    [[nodiscard]] OperationPtr get_operation(const std::string& operation_name = "") const {
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
};


/**
 * @brief GraphQL resolver registry for field resolution
 */
class ResolverRegistry {
public:
    using ResolverFunction = std::function<nlohmann::json(
        const nlohmann::json& parent,
        const nlohmann::json& context
    )>;
    
    /**
     * @brief Register field resolver
     * @param field_name Name of field to resolve
     * @param resolver Function to handle field resolution
     */
    void register_resolver(const std::string& field_name, ResolverFunction resolver) {
        resolvers_[field_name] = std::move(resolver);
    }
    
    /**
     * @brief Resolve field value
     * @param field_name Name of field to resolve
     * @param parent Parent object context
     * @param context Execution context
     * @return Resolved field value
     */
    [[nodiscard]] nlohmann::json resolve_field(
        const std::string& field_name,
        const nlohmann::json& parent,
        const nlohmann::json& context) const {
        
        auto it = resolvers_.find(field_name);
        if (it != resolvers_.end()) {
            return it->second(parent, context);
        }
        
        // Default resolver - try to extract field from parent object
        if (parent.is_object() && parent.contains(field_name)) {
            return parent[field_name];
        }
        
        return nlohmann::json();
    }
    
    /**
     * @brief Check if resolver exists for field
     * @param field_name Name of field to check
     * @return true if resolver is registered
     */
    [[nodiscard]] bool has_resolver(const std::string& field_name) const noexcept {
        return resolvers_.find(field_name) != resolvers_.end();
    }

private:
    std::unordered_map<std::string, ResolverFunction> resolvers_;
};

/**
 * @brief Main GraphQL executor class
 */
class GraphQLExecutor {
public:
    /**
     * @brief Execution configuration
     */
    struct Config {
        std::chrono::milliseconds timeout{5000};     ///< Execution timeout
        bool enable_introspection = true;            ///< Enable schema introspection
        uint32_t max_query_length = 100000;          ///< Maximum query string length
    };
    
    /**
     * @brief Construct GraphQL executor
     * @param database Database manager for data access
     * @param config Execution configuration
     */
    explicit GraphQLExecutor(std::shared_ptr<DatabaseManager> database);
    
    /**
     * @brief Destructor
     */
    ~GraphQLExecutor() noexcept = default;

    static std::unique_ptr<GraphQLExecutor> create(std::shared_ptr<DatabaseManager> database);

    // Non-copyable, movable
    GraphQLExecutor(const GraphQLExecutor&) = delete;
    GraphQLExecutor& operator=(const GraphQLExecutor&) = delete;
    GraphQLExecutor(GraphQLExecutor&&) = default;
    GraphQLExecutor& operator=(GraphQLExecutor&&) = default;
    
    /**
     * @brief Parse GraphQL query string
     * @param query Query string to parse
     * @return Parsed document or nullptr on error
     */
    [[nodiscard]] std::pair<DocumentPtr, std::vector<std::string>> parse(const std::string& query) const;
    
    /**
     * @brief Execute GraphQL query
     * @param query Query string
     * @param variables Query variables (optional)
     * @param operation_name Operation name (optional)
     * @param context Execution context
     * @return Execution result
     */
    [[nodiscard]] ExecutionResult execute(const std::string& query,
                                         const nlohmann::json& variables = {},
                                         const std::string& operation_name = "",
                                         const nlohmann::json& context = {}) const;

    /**
     * @brief Setup built-in resolvers
     */
    void setup_builtin_resolvers();

private:
    std::shared_ptr<DatabaseManager> database_;       ///< Database manager
    ResolverRegistry resolver_registry_;              ///< Field resolver registry
    
    /**
     * @brief Execute parsed operation
     * @param operation Operation to execute
     * @param variables Query variables
     * @param context Execution context
     * @return Operation result
     */
    [[nodiscard]] nlohmann::json execute_operation(
        const OperationPtr& operation,
        const nlohmann::json& variables,
        const nlohmann::json& context) const;
    
    /**
     * @brief Execute selection set
     * @param selections Selection set to execute
     * @param parent Parent object context
     * @param variables Query variables
     * @param context Execution context
     * @return Selection results
     */
    [[nodiscard]] nlohmann::json execute_selections(
        const std::vector<SelectionPtr>& selections,
        const nlohmann::json& parent,
        const nlohmann::json& variables,
        const nlohmann::json& context) const;
    
    /**
     * @brief Execute individual field
     * @param field Field to execute
     * @param parent Parent object context
     * @param variables Query variables
     * @param context Execution context
     * @return Field result
     */
    [[nodiscard]] nlohmann::json execute_field(
        const FieldPtr& field,
        const nlohmann::json& parent,
        const nlohmann::json& variables,
        const nlohmann::json& context) const;
    
};

} // namespace isched::v0_0_1::backend