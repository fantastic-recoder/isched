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
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <memory>
#include <format>
#include <unordered_map>
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
    class AuthenticationMiddleware;        ///< Forward declaration for login resolver
    class SubscriptionBroker;              ///< Forward declaration for event publishing
    class MetricsCollector;               ///< Forward declaration for metrics (T051)
    using DocumentPtr = std::shared_ptr<gql::Document>;

    using namespace std::chrono_literals; // enable 10ms, 5s, etc. literals in this header

    /**
     * Resolver context for GraphQL field resolution; provides access to the
     * current tenant, database connection, and authenticated user.
     *
     * Populated per-request by Server::execute_graphql() (or set directly in
     * tests) and threaded to every resolver via a thread-local slot.
     */
    struct ResolverCtx {
        std::string tenant_id;                          ///< Active tenant identifier
        std::weak_ptr<DatabaseManager> db;              ///< Non-owning handle to the tenant DB
        std::string current_user_id;                    ///< Authenticated user identifier (empty = anonymous)
        std::string user_name;                          ///< Human-readable display name (empty = anonymous)
        std::vector<std::string> roles;                 ///< Granted roles (e.g. "role_platform_admin")
        std::string session_id;                         ///< JWT @c jti claim for the current request; empty if unauthenticated
        std::string bearer_token;                       ///< Raw Bearer token for bearer_passthrough DataSource auth (T048-007)
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
            auto found = current->find(field_name);
            return found != current->end() && found->second.has_value();
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
            uint32_t max_depth       = 0;                ///< Max nesting depth (0 = unlimited, T041)
            uint32_t max_complexity  = 0;                ///< Max field count (0 = unlimited, T041)
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

        GqlExecutor(GqlExecutor&&) = delete;
        GqlExecutor& operator=(GqlExecutor&&) = delete;

        /**
         * @brief Parse GraphQL query string
         * @param pQuery Query string to parse
         * @return Parsed document or nullptr on error
         */
        [[nodiscard]] std::pair<DocumentPtr, std::vector<std::string>> parse(std::string&& pQuery) const;

        /**
         * @brief Execute GraphQL query
         * @param p_query Query string
         * @param p_variables_json Variables JSON object string (optional)
         * @param p_print_dot
         * @return Execution result
         */
        [[nodiscard]] ExecutionResult execute(std::string_view p_query,
                                              std::string_view p_variables_json = "{}",
                                              bool p_print_dot = false) const;

        /**
         * @brief Execute GraphQL query with an authenticated resolver context.
         *
         * Sets the thread-local resolver context (user id, roles, tenant) before
         * dispatching so that all resolvers invoked during this call see the
         * caller's authentication state.
         *
         * @param p_query           Query string
         * @param p_variables_json  Variables JSON object string
         * @param p_ctx             Populated auth/tenant context
         * @param p_print_dot       Verbose AST-dot flag
         */
        [[nodiscard]] ExecutionResult execute(std::string_view p_query,
                                              std::string_view p_variables_json,
                                              ResolverCtx p_ctx,
                                              bool p_print_dot = false) const;

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

        /**
         * @brief Declare which roles are required to call a top-level field.
         *
         * The check is OR-based: the caller must hold **at least one** of the
         * listed roles.  Registering an empty vector removes any prior gate.
         *
         * @param field_name  Top-level Query or Mutation field name.
         * @param roles       Allowed role strings (e.g. Role::PLATFORM_ADMIN).
         */
        void require_roles(const std::string& field_name,
                           std::vector<std::string> roles) {
            if (roles.empty()) {
                m_required_roles.erase(field_name);
            } else {
                m_required_roles[field_name] = std::move(roles);
            }
        }

        // ---------------------------------------------------------------
        // Pending-schema-change support (T034)
        // ---------------------------------------------------------------

        /**
         * @brief Describe a schema reload request queued by an activateSnapshot resolver.
         */
        struct PendingSchemaChange {
            std::string tenant_id;
            std::string new_sdl;
        };

        /**
         * @brief Retrieve any pending schema change set during the last execute() call.
         *
         * Returns nullopt if no activation was triggered.  Server::execute_graphql()
         * checks this after every execute() and calls load_schema() accordingly.
         */
        [[nodiscard]] std::optional<PendingSchemaChange> get_pending_schema_change() const {
            std::lock_guard<std::mutex> lk(m_pending_change_mutex);
            return m_pending_schema_change;
        }

        /** @brief Clear the pending schema change (called by Server after reload). */
        void clear_pending_schema_change() {
            std::lock_guard<std::mutex> lk(m_pending_change_mutex);
            m_pending_schema_change = std::nullopt;
        }

        /** @brief Store a pending schema change (called from within a resolver). */
        void set_pending_schema_change(PendingSchemaChange change) {
            std::lock_guard<std::mutex> lk(m_pending_change_mutex);
            m_pending_schema_change = std::move(change);
        }

        /** @brief Wire the subscription broker so resolvers can publish events (T046). */
        void set_subscription_broker(SubscriptionBroker* broker) {
            m_broker = broker;
        }

        /**
         * @brief Wire an AuthenticationMiddleware so the login resolver can issue JWTs.
         *
         * Must be called before the first request if the @c login mutation is used.
         * The executor holds a non-owning shared reference; the caller (Server) is
         * responsible for ensuring the middleware outlives the executor.
         */
        void set_auth_middleware(std::shared_ptr<AuthenticationMiddleware> auth) {
            m_auth = std::move(auth);
        }

        /**
         * @brief Provide the server-level master secret used to derive per-tenant
         *        AES-256-GCM keys for encrypting DataSource API keys (T048-001a).
         *
         * The JWT secret is a sensible default; call this before the first request
         * that touches createDataSource / updateDataSource.
         */
        void set_master_secret(std::string secret) {
            m_master_secret = std::move(secret);
        }

        /**
         * @brief Wire the MetricsCollector so resolvers can read live counters (T051).
         *
         * The collector is not owned by the executor; the caller (Server) is
         * responsible for ensuring the collector outlives the executor.
         */
        void set_metrics_collector(MetricsCollector* mc) {
            m_metrics = mc;
        }

        /**
         * @brief Register a callback invoked by the `shutdown` mutation after the
         *        response has been flushed (runs on a short-lived detached thread).
         *
         * Typically wired by the process entry-point to set a flag that causes the
         * main loop to exit cleanly.
         */
        void set_shutdown_callback(std::function<void()> cb) {
            m_shutdown_callback = std::move(cb);
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

        /// Per-field RBAC gates: field_name → list of permitted roles (OR logic).
        std::unordered_map<std::string, std::vector<std::string>> m_required_roles;

        // Pending schema change set by activateSnapshot resolver; consumed by Server.
        mutable std::mutex m_pending_change_mutex;
        std::optional<PendingSchemaChange> m_pending_schema_change;

        // Subscription broker for publishing events from resolvers (T046); not owned.
        SubscriptionBroker* m_broker{nullptr};

        // Metrics collector for live counter reads from resolvers (T051); not owned.
        MetricsCollector* m_metrics{nullptr};

        // Callback invoked by the shutdown mutation; set by the process entry-point.
        std::function<void()> m_shutdown_callback;

        // Authentication middleware for the login resolver (T047-016); shared ownership.
        std::shared_ptr<AuthenticationMiddleware> m_auth;

        // Server-level master secret for DataSource API key encryption (T048-001a).
        std::string m_master_secret;

        /// Execution configuration stored at construction time.
        Config m_config;

        using TTime = std::chrono::time_point<std::chrono::system_clock>;

        TTime m_start_time = std::chrono::system_clock::now();

        // ------------------------------------------------------------------
        // Query-parse cache: avoids re-parsing identical queries on each
        // request.  The parsed PEGTL AST holds const-char* iterators into
        // the string_input buffer, so both must be kept alive together.
        // ------------------------------------------------------------------

        /// FNV-1a–style transparent hash so string_view lookups don't
        /// allocate a std::string key.
        struct TransparentStringHash {
            using is_transparent = void;
            std::size_t operator()(std::string_view sv) const noexcept {
                return std::hash<std::string_view>{}(sv);
            }
        };

        struct CachedParse {
            /// Owns the query text (PEGTL nodes hold char* into this buffer).
            std::unique_ptr<tao::pegtl::string_input<>> input;
            /// Parse tree produced from the query above.
            gql::TAstNodePtr root;
        };

        static constexpr std::size_t k_query_cache_max = 512;

        mutable std::shared_mutex m_query_cache_mutex;
        mutable std::unordered_map<
            std::string,
            std::shared_ptr<CachedParse>,
            TransparentStringHash,
            std::equal_to<>>  m_query_cache;

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
            const nlohmann::json& p_parent,
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