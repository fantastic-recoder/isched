//
// Created by groby on 2025-12-19.
//

#ifndef ISCHED_GQL_PROCESSOR_HPP
#define ISCHED_GQL_PROCESSOR_HPP

#include <chrono>
#include <string>
#include <memory>
#include <nlohmann/json_fwd.hpp>

#include <tao/pegtl/parse_error.hpp>
#include <tao/pegtl/string_input.hpp>
#include <tao/pegtl/contrib/parse_tree.hpp>
#include <utility>

#include "isched_ExecutionResult.hpp"

namespace isched::v0_0_1::gql {
    struct Document;
}

namespace isched::v0_0_1::backend {
    class DatabaseManager;
    using DocumentPtr = std::shared_ptr<gql::Document>;

    using namespace std::chrono_literals; // enable 10ms, 5s, etc. literals in this header

    using ResolverFunction = std::function<nlohmann::json(
        const nlohmann::json& parent,
        const nlohmann::json& context
    )>;

    /**
     * @brief GraphQL resolver registry for field resolution
     */
    class ResolverRegistry {
    public:

        /**
         * @brief Register field resolver
         * @param field_name Name of field to resolve
         * @param resolver Function to handle field resolution
         */
        void register_resolver(const std::string& field_name, ResolverFunction&& resolver) {
            m_resolvers_map[field_name] = move(resolver);
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

            auto it = m_resolvers_map.find(field_name);
            if (it != m_resolvers_map.end()) {
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
         *
         */
        [[nodiscard]] bool has_resolver(const std::string& field_name) const noexcept {
            return m_resolvers_map.find(field_name) != m_resolvers_map.end();
        }

        const ResolverFunction & get_resolver(const std::string & p_name) const {
            return m_resolvers_map.find(p_name)->second;
        }

    private:
        std::unordered_map<std::string, ResolverFunction> m_resolvers_map;
    };

    /**
     * @brief Main GraphQL executor class
     */
    class GqlExecutor {
    public:
        /**
         * @brief Execution configuration
         */
        struct Config {
            std::chrono::milliseconds timeout = std::chrono::milliseconds{5000};     ///< Execution timeout
            bool enable_introspection = true;            ///< Enable schema introspection
            uint32_t max_query_length = 100000;          ///< Maximum query string length
        };

        static std::unique_ptr<GqlExecutor> create(std::shared_ptr<DatabaseManager> database);

        /**
         * @brief Construct GraphQL executor
         * @param database Database manager for data access
         * @param config Execution configuration
         */
        GqlExecutor(std::shared_ptr<DatabaseManager> database, Config config);

        /**
         * @brief Construct GraphQL executor with default configuration
         * @param database Database manager for data access
         */
        explicit GqlExecutor(std::shared_ptr<DatabaseManager> database);

        // Non-copyable, movable
        GqlExecutor(const GqlExecutor&) = delete;
        GqlExecutor& operator=(const GqlExecutor&) = delete;

        void setup_builtin_resolvers();

        GqlExecutor(GqlExecutor&&) = default;
        GqlExecutor& operator=(GqlExecutor&&) = default;

        /**
         * @brief Parse GraphQL query string
         * @param pQuery Query string to parse
         * @return Parsed document or nullptr on error
         */
        [[nodiscard]] std::pair<DocumentPtr, std::vector<std::string>> parse(std::string&& pQuery) const;

        /**
         * @brief Execute GraphQL query
         * @param p_query Query string
         * @param p_print_dot
         * @param p_print_dot
         * @return Execution result
         */
        [[nodiscard]] ExecutionResult execute(std::string_view p_query, bool p_print_dot=false) const;

        ExecutionResult load_schema(const std::string &pSchemaDocument, bool p_print_dot=false);

        void log_parse_error_exception(const tao::pegtl::string_input<> &  in, ExecutionResult myResult,
                                       const tao::pegtl::parse_error &e) const;


        void register_resolver(const std::string& field_name, ResolverFunction&& resolver) {
            m_resolvers.register_resolver(field_name, move(resolver));
        }

    private:
        using TNodePtr = std::vector<std::unique_ptr<tao::pegtl::parse_tree::node>>::value_type;

        ResolverRegistry m_resolvers;
        TNodePtr m_current_schema;

        void process_field_definition(ExecutionResult &p_result,
                                      const TNodePtr &p_typedef,
                                      size_t p_idx);

        bool process_operation_definitions(ExecutionResult &p_result,
                                           const TNodePtr &myOperation) const;

        nlohmann::json extract_argument_value(const TNodePtr &p_arg, ExecutionResult &p_execution_result) const;

        nlohmann::json process_arguments(const TNodePtr & p_field_node, ExecutionResult & p_execution_result) const;

        void process_field_selection(const TNodePtr &p_selection_set, ExecutionResult &p_result) const;
    };
}
// isched::v0_0_1

#endif //ISCHED_GQL_PROCESSOR_HPP