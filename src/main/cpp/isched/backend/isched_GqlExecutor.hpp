//
// Created by groby on 2025-12-19.
//

#ifndef ISCHED_GQL_PROCESSOR_HPP
#define ISCHED_GQL_PROCESSOR_HPP

#include <chrono>
#include <string>
#include <memory>
#include <nlohmann/json_fwd.hpp>

#include "isched_ExecutionResult.hpp"

namespace isched::v0_0_1::gql {
    struct Document;
}

namespace isched::v0_0_1::backend {
    class DatabaseManager;
    using DocumentPtr = std::shared_ptr<gql::Document>;

    using namespace std::chrono_literals; // enable 10ms, 5s, etc. literals in this header

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
        explicit GqlExecutor(std::shared_ptr<DatabaseManager> database, Config config);

        /**
         * @brief Construct GraphQL executor with the default configuration
         * @param database Database manager for data access
         */
        explicit GqlExecutor(std::shared_ptr<DatabaseManager> database) {
            GqlExecutor(database, Config{});
        }


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
         * @param p_variables Query variables (optional)
         * @param p_operation_name Operation name (optional)
         * @param p_context Execution context
         * @return Execution result
         */
        [[nodiscard]] ExecutionResult execute(const std::string& p_query,
                                                      const nlohmann::json& p_variables = {},
                                                      const std::string& p_operation_name = "",
                                                      const nlohmann::json& p_context = {}) const;

        ExecutionResult execute(gql::Document p_doc);
    private:
    };
}
// isched::v0_0_1

#endif //ISCHED_GQL_PROCESSOR_HPP