// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_GqlExecutor.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief GraphQL query/mutation executor and built-in resolver registry.
 *
 * Declares `GqlExecutor`, which owns the resolver map, dispatches incoming
 * GraphQL operations to registered handler lambdas, and provides built-in
 * schema fields (`hello`, `version`, `uptime`, `serverInfo`, `health`,
 * `metrics`, `env`, `configprops`) plus a skeleton introspection
 * implementation (`__schema`, `__type`).
 *
 * @see isched_gql_grammar.hpp  PEGTL grammar consumed by this class.
 */

#ifndef ISCHED_GQL_PROCESSOR_HPP
#define ISCHED_GQL_PROCESSOR_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <memory>
#include <format>
#include <nlohmann/json_fwd.hpp>

#include <string_view>
#include <tao/pegtl/parse_error.hpp>
#include <tao/pegtl/string_input.hpp>
#include <utility>
#include <vector>

#include "isched_ExecutionResult.hpp"
#include "isched_gql_error.hpp"
#include "isched_gql_grammar.hpp"
#include "isched_log_result.hpp"
#include "isched_multi_dim_map.hpp"

namespace Catch {
    class Context;
}

namespace isched::v0_0_1::gql {
    struct Document;
}

namespace isched::v0_0_1::backend {
    class DatabaseManager;
    using DocumentPtr = std::shared_ptr<gql::Document>;

    using namespace std::chrono_literals; // enable 10ms, 5s, etc. literals in this header

    /**
     * Resolver context for GraphQL field resolution; will provide access to the database and other resources
     */
    struct ResolverCtx {
    };

    /**
     * p_parent - parents resolver result for field resolution
     * p_args - arguments passed to resolver
     * p_ctx - context passed to resolver
     */
    using ResolverFunction = std::function<nlohmann::json(
        const nlohmann::json& p_parent,
        const nlohmann::json& p_args,
        const ResolverCtx& p_ctx
    )>;

    using ResolverPath = std::vector<std::string>;

    /**
     * @brief GraphQL resolver registry for field resolution
     */
    class ResolverRegistry {
    public:

        /**
         * @brief Register field resolver
         * @param p_scope parent scope/resolver path
         * @param p_field_name Name of field to resolve
         * @param p_resolver Function to handle field resolution
         */
        void register_resolver(const ResolverPath& p_scope, const std::string& p_field_name, ResolverFunction &&p_resolver) {
            m_resolvers_map[p_scope][p_field_name]= p_resolver;
        }

        /**
         * @brief Resolve field value
         * @param field_name Name of field to resolve
         * @param parent Parent resolver result
         * @param args Arguments passed to resolver
         * @param context Execution context
         * @return Resolved field value
         */
        [[nodiscard]] nlohmann::json resolve_field(
            const std::string& field_name,
            const nlohmann::json& parent,
            const nlohmann::json& args,
            const ResolverCtx& context) const {

            auto it = m_resolvers_map.find(field_name);
            if (it != m_resolvers_map.end()) {
                return it->second.get_value()(parent, args, context);
            }

            // Default resolver - try to extract field from p_args object
            if (parent.is_object() && parent.contains(field_name)) {
                return parent[field_name];
            }

            return {};
        }

        /**
         * @brief Check if resolver exists for field
         * @param field_name Name of field to check
         * @return true if resolver is registered
         *
         */
        [[nodiscard]] bool has_resolver(const ResolverPath& p_path, const std::string& field_name) const noexcept {
            const multi_dim_map<std::string, ResolverFunction>* current = &m_resolvers_map;
            for (const auto& key : p_path) {
                auto it = current->find(key);
                if (it == current->end()) {
                    return false;
                }
                current = &(it->second);
            }
            return current->find(field_name) != current->end();
        }

        [[nodiscard]] const ResolverFunction & get_resolver(const ResolverPath& p_path, const std::string & p_name) const {
            const multi_dim_map<std::string, ResolverFunction>* current = &m_resolvers_map;
            for (const auto& key : p_path) {
                auto it = current->find(key);
                if (it == current->end()) {
                    throw std::out_of_range("Resolver path not found");
                }
                current = &(it->second);
            }
            auto it = current->find(p_name);
            if (it == current->end()) {
                throw std::out_of_range("Resolver not found: " + p_name);
            }
            return it->second.get_value();
        }

    private:
        multi_dim_map<std::string, ResolverFunction> m_resolvers_map;
    };

    /**
     * @brief Main GraphQL executor class
     */
    class GqlExecutor {
    public:

        using TDbManagerPtr = std::shared_ptr<DatabaseManager>;

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
         * @param p_database Database manager for data access
         * @param config Execution configuration
         */
        GqlExecutor(std::shared_ptr<DatabaseManager> p_database, Config config);

        /**
         * @brief Construct GraphQL executor with default configuration
         * @param database Database manager for data access
         */
        explicit GqlExecutor(std::shared_ptr<DatabaseManager> database);

        // Non-copyable, movable
        GqlExecutor(const GqlExecutor&) = delete;
        GqlExecutor& operator=(const GqlExecutor&) = delete;
        ~GqlExecutor() = default;

        void setup_builtin_resolvers();

        nlohmann::json generate_directives_introspection();

        nlohmann::json generate_schema_introspection();

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
        [[nodiscard]] ExecutionResult execute(std::string_view p_query, bool p_print_dot = false) const;

        /**
         *
         * @param pSchemaDocument input schema definition, it will be consumed
         *
         * @param p_print_dot verbose flag
         *
         * @return the query execution result
         */
        ExecutionResult load_schema(std::string &&pSchemaDocument, bool p_print_dot = false);

        ExecutionResult load_schema(const std::string &pSchemaDocument, bool p_print_dot = false) {
            return load_schema(std::string(pSchemaDocument),p_print_dot);
        }

        void log_parse_error_exception(const tao::pegtl::string_input<> &  in, ExecutionResult &myResult,
                                       const tao::pegtl::parse_error &e) const;


        void register_resolver(const ResolverPath& path, const std::string& field_name, ResolverFunction&& resolver) {
            if (m_resolvers.has_resolver(path,field_name)) {
                throw std::runtime_error(std::format("GraphQL resolver \"{}.{}\" already registered.",
                    concat_vector(path,"."),field_name));
            };
            m_resolvers.register_resolver(path, field_name, std::move(resolver));
        }

    private:

        using TAstNodeMap = std::map<std::string, const gql::TAstNodePtr*>;
        using TSchemaDoc = tao::pegtl::string_input<>;
        using TSchemaDocPtr = std::shared_ptr<TSchemaDoc>;

        ResolverRegistry m_resolvers;
        gql::TAstNodePtr m_current_schema;
        TDbManagerPtr m_database;
        TAstNodeMap m_type_map;
        TAstNodeMap m_directives;
        std::vector<TSchemaDocPtr> m_schema_documents;

        using TTime = std::chrono::time_point<std::chrono::system_clock>;

        TTime m_start_time = std::chrono::system_clock::now();

        void update_type_map_recursive(const gql::TAstNodePtr &p_typedef, TAstNodeMap &p_type_map, TAstNodeMap &p_directives);

        void update_type_map();

        void process_field_definition(const ResolverPath &p_path, ExecutionResult &p_result, const gql::TAstNodePtr &p_typedef, size_t p_idx);

        bool process_operation_definitions(
            nlohmann::json p_parent_result, const gql::TAstNodePtr &myOperation, nlohmann::json &p_result, gql::TErrorVector &
            p_errors) const;

        nlohmann::json extract_argument_value(const gql::TAstNodePtr &p_arg, gql::TErrorVector &p_errors) const;

        nlohmann::json process_arguments(const gql::TAstNodePtr &p_field_node,
                                         gql::TErrorVector &p_errors) const;

        nlohmann::json process_argument_field(
            const gql::TAstNodePtr &p_field_node,
            gql::TErrorVector &p_error) const;

        void process_sub_selection(
            const nlohmann::json &p_parent_result,
            const ResolverPath &p_path, const gql::TAstNodePtr &node,
            nlohmann::json &p_result,
            gql::TErrorVector &p_errors) const;

        void process_field_sub_selections(
            const nlohmann::json &p_parent_result,
            const ResolverPath &p_path,
            const gql::TAstNodePtr &p_selection_set,
            nlohmann::json &p_result,
            gql::TErrorVector &p_error,
            std::string myFieldName
        ) const;

        bool resolve_field_selection_details(
            const ResolverPath &p_path,
            const gql::TAstNodePtr &p_field_node, nlohmann::json &p_result, gql::TErrorVector &p_error) const;

        void process_field_selection(
            const nlohmann::json &p_parent_result,
            const ResolverPath &p_path, const gql::TAstNodePtr &p_selection_set, nlohmann::json &p_result, gql::TErrorVector &
            p_errors) const;

        [[nodiscard]] gql::TAstNodePtr const * find_node_by_type(const TAstNodeMap::value_type &pair, std::string_view p_type) const ;

    };
}
// isched::v0_0_1

#endif //ISCHED_GQL_PROCESSOR_HPP