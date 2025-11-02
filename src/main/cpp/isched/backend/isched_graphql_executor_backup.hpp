/**
 * @file isched_graphql_executor.hpp
 * @brief GraphQL query executor with smart pointer-based AST
 * @author Isched Development Team
 * @date 2024-12-20
 * @version 1.0.0
 */

#pragma once

#include "isched_common.hpp"
#include "isched_database.hpp"
#include <tao/pegtl.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <variant>
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
 * @brief GraphQL scalar value types
 */
using GraphQLValue = std::variant<
    std::nullptr_t,
    bool,
    int64_t,
    double,
    std::string,
    std::vector<std::shared_ptr<std::variant<std::nullptr_t, bool, int64_t, double, std::string>>>,
    std::unordered_map<std::string, std::shared_ptr<std::variant<std::nullptr_t, bool, int64_t, double, std::string>>>
>;

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
 * @brief GraphQL argument with name and value
 */
class Argument final : public ASTNode {
public:
    std::string name;                                 ///< Argument name
    GraphQLValue value;                               ///< Argument value
    
    Argument(std::string arg_name, GraphQLValue arg_value) 
        : name(std::move(arg_name)), value(std::move(arg_value)) {}
    
    [[nodiscard]] std::string get_type() const override { return "Argument"; }
    
    void accept(const std::function<void(const ASTNode&)>& visitor) const override {
        visitor(*this);
    }
};

using ArgumentPtr = std::shared_ptr<Argument>;

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
    std::vector<ArgumentPtr> arguments;               ///< Field arguments
    std::vector<SelectionPtr> selection_set;          ///< Nested field selections
    
    explicit Field(std::string field_name) : name(std::move(field_name)) {}
    
    [[nodiscard]] std::string get_type() const override { return "Field"; }
    
    void accept(const std::function<void(const ASTNode&)>& visitor) const override;
    
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
    [[nodiscard]] OperationPtr get_operation(const std::string& operation_name = "") const;
};

/**
 * @brief GraphQL execution result
 */
struct ExecutionResult {
    nlohmann::json data;                             ///< Query result data
    std::vector<std::string> errors;                 ///< Execution errors
    nlohmann::json extensions;                       ///< Optional extensions
    std::chrono::milliseconds execution_time{0};     ///< Execution duration
    
    /**
     * @brief Convert result to JSON response
     * @return JSON response according to GraphQL spec
     */
    [[nodiscard]] nlohmann::json to_json() const;
    
    /**
     * @brief Check if execution was successful
     * @return true if no errors occurred
     */
    [[nodiscard]] bool is_success() const noexcept {
        return errors.empty();
    }
};

/**
 * @brief Query complexity analyzer for preventing DoS attacks
 */
class QueryComplexityAnalyzer {
public:
    struct ComplexityConfig {
        uint32_t max_depth = 15;                     ///< Maximum query depth
        uint32_t max_complexity = 1000;              ///< Maximum complexity score
        uint32_t max_fields = 100;                   ///< Maximum field count
    };
    
    struct ComplexityResult {
        uint32_t depth = 0;                          ///< Query depth
        uint32_t complexity = 0;                     ///< Complexity score
        uint32_t field_count = 0;                    ///< Total field count
        bool is_valid = true;                        ///< Within limits
        std::string error_message;                   ///< Error if invalid
    };
    
    explicit QueryComplexityAnalyzer(ComplexityConfig config = {15, 1000, 100})
        : config_(config) {}
    
    /**
     * @brief Analyze query complexity
     * @param document Parsed GraphQL document
     * @param operation_name Name of operation to analyze
     * @return Complexity analysis result
     */
    [[nodiscard]] ComplexityResult analyze(const DocumentPtr& document, 
                                          const std::string& operation_name = "") const;

private:
    ComplexityConfig config_;
    
    /**
     * @brief Calculate complexity for selection set
     * @param selections Selection set to analyze
     * @param depth Current depth
     * @return Complexity metrics
     */
    [[nodiscard]] ComplexityResult analyze_selections(
        const std::vector<SelectionPtr>& selections, 
        uint32_t depth) const;
};

/**
 * @brief GraphQL resolver registry for field resolution
 */
class ResolverRegistry {
public:
    using ResolverFunction = std::function<nlohmann::json(
        const nlohmann::json& parent,
        const std::vector<ArgumentPtr>& args,
        const nlohmann::json& context
    )>;
    
    /**
     * @brief Register field resolver
     * @param field_name Name of field to resolve
     * @param resolver Function to handle field resolution
     */
    void register_resolver(const std::string& field_name, ResolverFunction resolver);
    
    /**
     * @brief Resolve field value
     * @param field_name Name of field to resolve
     * @param parent Parent object context
     * @param arguments Field arguments
     * @param context Execution context
     * @return Resolved field value
     */
    [[nodiscard]] nlohmann::json resolve_field(
        const std::string& field_name,
        const nlohmann::json& parent,
        const std::vector<ArgumentPtr>& arguments,
        const nlohmann::json& context) const;
    
    /**
     * @brief Check if resolver exists for field
     * @param field_name Name of field to check
     * @return true if resolver is registered
     */
    [[nodiscard]] bool has_resolver(const std::string& field_name) const noexcept;

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
        QueryComplexityAnalyzer::ComplexityConfig complexity;  ///< Complexity limits
        bool enable_introspection = true;            ///< Enable schema introspection
        uint32_t max_query_length = 100000;          ///< Maximum query string length
    };
    
    /**
     * @brief Construct GraphQL executor
     * @param database Database manager for data access
     * @param config Execution configuration
     */
    explicit GraphQLExecutor(std::shared_ptr<DatabaseManager> database,
                           Config config = {{5000}, {15, 1000, 100}, true, 100000});
    
    /**
     * @brief Destructor
     */
    ~GraphQLExecutor() noexcept = default;
    
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

private:
    std::shared_ptr<DatabaseManager> database_;       ///< Database manager
    Config config_;                                   ///< Execution configuration  
    QueryComplexityAnalyzer complexity_analyzer_;     ///< Query complexity analyzer
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
    
    /**
     * @brief Setup built-in resolvers
     */
    void setup_builtin_resolvers();
};

} // namespace isched::v0_0_1::backend
 * This file implements a comprehensive GraphQL query parser and executor using the
 * PEGTL (Parsing Expression Grammar Template Library) for parsing GraphQL syntax.
 * All memory management uses smart pointers following C++ Core Guidelines.
 * 
 * @copyright Copyright (c) 2024 Isched Development Team
 * @license MIT License
 */

#pragma once

#include "isched_common.hpp"
#include "isched_database.hpp"
#include <tao/pegtl.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>
#include <functional>
#include <chrono>

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
 * @brief GraphQL scalar value types
 */
using GraphQLValue = std::variant<
    std::nullptr_t,
    bool,
    int64_t,
    double,
    std::string,
    std::vector<std::shared_ptr<std::variant<std::nullptr_t, bool, int64_t, double, std::string>>>,
    std::unordered_map<std::string, std::shared_ptr<std::variant<std::nullptr_t, bool, int64_t, double, std::string>>>
>;

/**
 * @brief Result of GraphQL execution
 */
struct ExecutionResult {
    nlohmann::json data;                              ///< Query result data
    std::vector<nlohmann::json> errors;               ///< Execution errors
    nlohmann::json extensions;                        ///< Result extensions
    std::chrono::microseconds execution_time;         ///< Total execution time
    
    /**
     * @brief Check if execution was successful
     * @return true if no errors occurred
     */
    [[nodiscard]] bool is_success() const noexcept {
        return errors.empty();
    }
    
    /**
     * @brief Convert to JSON response format per GraphQL specification
     * @return JSON formatted response
     */
    [[nodiscard]] nlohmann::json to_json() const;
};

/**
 * @brief Base class for all AST nodes with smart pointer support
 */
class ASTNode {
public:
    virtual ~ASTNode() = default;
    
    /**
     * @brief Get node type for runtime identification
     * @return String representation of node type
     */
    [[nodiscard]] virtual std::string get_type() const = 0;
    
    /**
     * @brief Accept visitor pattern for AST traversal
     * @param visitor Function to apply to this node
     */
    virtual void accept(const std::function<void(const ASTNode&)>& visitor) const = 0;
    
protected:
    ASTNode() = default;
    ASTNode(const ASTNode&) = default;
    ASTNode(ASTNode&&) = default;
    ASTNode& operator=(const ASTNode&) = default;
    ASTNode& operator=(ASTNode&&) = default;
};

/**
 * @brief GraphQL argument with name and value
 */
class Argument final : public ASTNode {
public:
    std::string name;
    GraphQLValue value;
    
    explicit Argument(std::string arg_name, GraphQLValue arg_value)
        : name(std::move(arg_name)), value(std::move(arg_value)) {}
    
    [[nodiscard]] std::string get_type() const override { return "Argument"; }
    
    void accept(const std::function<void(const ASTNode&)>& visitor) const override {
        visitor(*this);
    }
};

using ArgumentPtr = std::shared_ptr<Argument>;

/**
 * @brief GraphQL field with arguments and selections
 */
class Field final : public ASTNode {
public:
    std::string name;                                 ///< Field name
    std::string alias;                                ///< Optional field alias
    std::vector<ArgumentPtr> arguments;               ///< Field arguments
    std::vector<SelectionPtr> selection_set;          ///< Nested field selections
    
    explicit Field(std::string field_name) : name(std::move(field_name)) {}
    
    [[nodiscard]] std::string get_type() const override { return "Field"; }
    
    void accept(const std::function<void(const ASTNode&)>& visitor) const override;
    
    /**
     * @brief Get effective field name (alias if present, otherwise name)
     * @return The name to use in response
     */
    [[nodiscard]] const std::string& get_response_key() const noexcept {
        return alias.empty() ? name : alias;
    }
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
    [[nodiscard]] OperationPtr get_operation(const std::string& operation_name = "") const;
};

/**
 * @brief GraphQL parsing grammar using PEGTL
 */
namespace grammar {
    namespace pegtl = tao::pegtl;
    
    // Forward declarations for PEGTL rules
    struct Document;
    struct OperationDefinition;
    struct SelectionSet;
    struct Field;
    struct Arguments;
    struct Argument;
    struct Value;
    struct Name;
    struct StringValue;
    struct IntValue;
    struct FloatValue;
    struct BooleanValue;
    struct NullValue;
    struct Comment;
    struct Whitespace;
    
    // Basic character classes
    struct NameStart : pegtl::ranges<'A', 'Z', 'a', 'z', '_'> {};
    struct NameContinue : pegtl::ranges<'A', 'Z', 'a', 'z', '0', '9', '_'> {};
    
    // Whitespace and ignored tokens
    struct LineTerminator : pegtl::eol {};
    struct WhitespaceChar : pegtl::one<' ', '\t'> {};
    struct CommentChar : pegtl::seq<pegtl::one<'#'>, pegtl::until<LineTerminator>> {};
    struct Ignored : pegtl::sor<WhitespaceChar, LineTerminator, CommentChar> {};
    struct OptionalIgnored : pegtl::star<Ignored> {};
    
    // Names and keywords
    struct Name : pegtl::seq<NameStart, pegtl::star<NameContinue>> {};
    struct QueryKeyword : pegtl::string<'q', 'u', 'e', 'r', 'y'> {};
    struct MutationKeyword : pegtl::string<'m', 'u', 't', 'a', 't', 'i', 'o', 'n'> {};
    struct SubscriptionKeyword : pegtl::string<'s', 'u', 'b', 's', 'c', 'r', 'i', 'p', 't', 'i', 'o', 'n'> {};
    struct TrueKeyword : pegtl::string<'t', 'r', 'u', 'e'> {};
    struct FalseKeyword : pegtl::string<'f', 'a', 'l', 's', 'e'> {};
    struct NullKeyword : pegtl::string<'n', 'u', 'l', 'l'> {};
    
    // String literals
    struct EscapedChar : pegtl::seq<pegtl::one<'\\'>, pegtl::one<'"', '\\', '/', 'b', 'f', 'n', 'r', 't'>> {};
    struct StringChar : pegtl::sor<EscapedChar, pegtl::ranges<0x20, 0x21, 0x23, 0x5B, 0x5D, 0x7E>> {};
    struct StringValue : pegtl::seq<pegtl::one<'"'>, pegtl::star<StringChar>, pegtl::one<'"'>> {};
    
    // Numeric literals
    struct IntValue : pegtl::seq<pegtl::opt<pegtl::one<'-'>>, pegtl::plus<pegtl::digit>> {};
    struct FloatValue : pegtl::seq<
        pegtl::opt<pegtl::one<'-'>>,
        pegtl::plus<pegtl::digit>,
        pegtl::one<'.'>,
        pegtl::plus<pegtl::digit>,
        pegtl::opt<pegtl::seq<pegtl::one<'e', 'E'>, pegtl::opt<pegtl::one<'+', '-'>>, pegtl::plus<pegtl::digit>>>
    > {};
    
    // Boolean and null values
    struct BooleanValue : pegtl::sor<TrueKeyword, FalseKeyword> {};
    struct NullValue : NullKeyword {};
    
    // Values
    struct Value : pegtl::sor<StringValue, FloatValue, IntValue, BooleanValue, NullValue> {};
    
    // Arguments
    struct Argument : pegtl::seq<Name, OptionalIgnored, pegtl::one<':'>, OptionalIgnored, Value> {};
    struct Arguments : pegtl::seq<
        pegtl::one<'('>, OptionalIgnored,
        pegtl::opt<pegtl::seq<Argument, pegtl::star<pegtl::seq<OptionalIgnored, Argument>>>>,
        OptionalIgnored, pegtl::one<')'>
    > {};
    
    // Field alias
    struct FieldAlias : pegtl::seq<Name, OptionalIgnored, pegtl::one<':'>, OptionalIgnored> {};
    
    // Forward declaration for recursive rules
    struct SelectionSet;
    
    // Field definition
    struct GrammarField : pegtl::seq<
        pegtl::opt<FieldAlias>,
        Name,
        pegtl::opt<Arguments>,
        pegtl::opt<SelectionSet>
    > {};
    
    // Selection set
    struct SelectionSet : pegtl::seq<
        pegtl::one<'{'>, OptionalIgnored,
        pegtl::opt<pegtl::seq<GrammarField, pegtl::star<pegtl::seq<OptionalIgnored, GrammarField>>>>,
        OptionalIgnored, pegtl::one<'}'>
    > {};
    
    // Operation type
    struct GrammarOperationType : pegtl::sor<QueryKeyword, MutationKeyword, SubscriptionKeyword> {};
    
    // Operation definition
    struct GrammarOperationDefinition : pegtl::seq<
        pegtl::opt<pegtl::seq<GrammarOperationType, OptionalIgnored>>,
        pegtl::opt<pegtl::seq<Name, OptionalIgnored>>,
        SelectionSet
    > {};
    
    // Document
    struct GrammarDocument : pegtl::seq<
        OptionalIgnored,
        pegtl::opt<pegtl::seq<GrammarOperationDefinition, pegtl::star<pegtl::seq<OptionalIgnored, GrammarOperationDefinition>>>>,
        OptionalIgnored,
        pegtl::eof
    > {};
    
} // namespace grammar

/**
 * @brief Parser state for building AST during parsing
 */
class ParseState {
public:
    DocumentPtr document;
    std::vector<std::string> errors;
    
    ParseState() : document(std::make_shared<Document>()) {}
    
    /**
     * @brief Add parsing error
     * @param error Error message
     */
    void add_error(const std::string& error) {
        errors.push_back(error);
    }
    
    /**
     * @brief Check if parsing was successful
     * @return true if no errors occurred
     */
    [[nodiscard]] bool is_success() const noexcept {
        return errors.empty();
    }
};

/**
 * @brief Query complexity analyzer for preventing DoS attacks
 */
class QueryComplexityAnalyzer {
public:
    struct ComplexityConfig {
        uint32_t max_depth = 15;                     ///< Maximum query depth
        uint32_t max_complexity = 1000;              ///< Maximum complexity score
        uint32_t max_fields = 100;                   ///< Maximum field count
    };
    
    struct ComplexityResult {
        uint32_t depth = 0;                          ///< Query depth
        uint32_t complexity = 0;                     ///< Complexity score
        uint32_t field_count = 0;                    ///< Total field count
        bool is_valid = true;                        ///< Within limits
        std::string error_message;                   ///< Error if invalid
    };
    
    explicit QueryComplexityAnalyzer(ComplexityConfig config = {15, 1000, 100})
        : config_(config) {}
    
    /**
     * @brief Analyze query complexity
     * @param document Parsed GraphQL document
     * @param operation_name Name of operation to analyze
     * @return Complexity analysis result
     */
    [[nodiscard]] ComplexityResult analyze(const DocumentPtr& document, 
                                          const std::string& operation_name = "") const;

private:
    ComplexityConfig config_;
    
    /**
     * @brief Calculate complexity for selection set
     * @param selections Selection set to analyze
     * @param depth Current depth
     * @return Complexity metrics
     */
    [[nodiscard]] ComplexityResult analyze_selections(
        const std::vector<SelectionPtr>& selections, uint32_t depth) const;
};

/**
 * @brief Resolver function type for field resolution
 */
using ResolverFunction = std::function<nlohmann::json(
    const FieldPtr& field,
    const nlohmann::json& parent,
    const nlohmann::json& context
)>;

/**
 * @brief Field resolver registry
 */
class ResolverRegistry {
public:
    /**
     * @brief Register resolver for a field
     * @param type_name GraphQL type name
     * @param field_name Field name
     * @param resolver Resolver function
     */
    void register_resolver(const std::string& type_name,
                          const std::string& field_name,
                          ResolverFunction resolver);
    
    /**
     * @brief Get resolver for a field
     * @param type_name GraphQL type name
     * @param field_name Field name
     * @return Resolver function or nullptr if not found
     */
    [[nodiscard]] ResolverFunction get_resolver(const std::string& type_name,
                                               const std::string& field_name) const;
    
    /**
     * @brief Check if resolver exists
     * @param type_name GraphQL type name
     * @param field_name Field name
     * @return true if resolver is registered
     */
    [[nodiscard]] bool has_resolver(const std::string& type_name,
                                   const std::string& field_name) const;

private:
    std::unordered_map<std::string, std::unordered_map<std::string, ResolverFunction>> resolvers_;
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
        QueryComplexityAnalyzer::ComplexityConfig complexity;  ///< Complexity limits
        bool enable_introspection = true;            ///< Enable schema introspection
        uint32_t max_query_length = 100000;          ///< Maximum query string length
    };
    
    /**
     * @brief Construct GraphQL executor
     * @param database Database manager for data access
     * @param config Execution configuration
     */
    explicit GraphQLExecutor(std::shared_ptr<DatabaseManager> database,
                           Config config = {{5000}, {15, 1000, 100}, true, 100000});
    
    /**
     * @brief Destructor
     */
    ~GraphQLExecutor() noexcept = default;
    
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
     * @brief Execute parsed document
     * @param document Parsed GraphQL document
     * @param variables Query variables
     * @param operation_name Operation name
     * @param context Execution context
     * @return Execution result
     */
    [[nodiscard]] ExecutionResult execute_document(const DocumentPtr& document,
                                                  const nlohmann::json& variables = {},
                                                  const std::string& operation_name = "",
                                                  const nlohmann::json& context = {}) const;
    
    /**
     * @brief Get resolver registry for registering custom resolvers
     * @return Reference to resolver registry
     */
    [[nodiscard]] ResolverRegistry& get_resolver_registry() noexcept {
        return resolvers_;
    }
    
    /**
     * @brief Get resolver registry (const)
     * @return Const reference to resolver registry
     */
    [[nodiscard]] const ResolverRegistry& get_resolver_registry() const noexcept {
        return resolvers_;
    }

private:
    std::shared_ptr<DatabaseManager> database_;       ///< Database access
    Config config_;                                   ///< Executor configuration
    ResolverRegistry resolvers_;                      ///< Field resolvers
    QueryComplexityAnalyzer complexity_analyzer_;     ///< Complexity analyzer
    
    /**
     * @brief Execute operation
     * @param operation Operation to execute
     * @param variables Query variables
     * @param context Execution context
     * @return Execution result data
     */
    [[nodiscard]] nlohmann::json execute_operation(const OperationPtr& operation,
                                                   const nlohmann::json& variables,
                                                   const nlohmann::json& context) const;
    
    /**
     * @brief Execute selection set
     * @param selections Selection set to execute
     * @param parent_value Parent object value
     * @param parent_type Parent type name
     * @param context Execution context
     * @return Result object
     */
    [[nodiscard]] nlohmann::json execute_selection_set(
        const std::vector<SelectionPtr>& selections,
        const nlohmann::json& parent_value,
        const std::string& parent_type,
        const nlohmann::json& context) const;
    
    /**
     * @brief Execute single field
     * @param field Field to execute
     * @param parent_value Parent object value
     * @param parent_type Parent type name
     * @param context Execution context
     * @return Field result value
     */
    [[nodiscard]] nlohmann::json execute_field(const FieldPtr& field,
                                               const nlohmann::json& parent_value,
                                               const std::string& parent_type,
                                               const nlohmann::json& context) const;
    
    /**
     * @brief Setup built-in resolvers (hello, version, etc.)
     */
    void setup_builtin_resolvers();
};

} // namespace isched::v0_0_1::backend