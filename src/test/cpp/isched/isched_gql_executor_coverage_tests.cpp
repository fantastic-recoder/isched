// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_gql_executor_coverage_tests.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @brief Catch2 unit tests for GqlExecutor complexity/depth, schema upload, and coverage.
 */

#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <variant>
#include <iostream>
#include <chrono>

#include <isched/backend/isched_GqlExecutor.hpp>
#include <isched/backend/isched_gql_grammar.hpp>
#include <nlohmann/json_fwd.hpp>
#include <tao/pegtl/string_input.hpp>
#include <functional>
#include <vector>

#include "isched/backend/isched_log_result.hpp"
#include "isched/backend/isched_DatabaseManager.hpp"
#include "isched/shared/fs/isched_fs_utils.hpp"

using nlohmann::json;
using isched::v0_0_1::gql::EErrorCodes;

namespace isched::v0_0_1::backend {

    namespace {

        constexpr std::string_view k_schema_upload_test_sdl = "type Query { ping: String }";

        struct ScopedEnvVar {
            explicit ScopedEnvVar(const char* name)
                : name_(name) {
                const char* existing = std::getenv(name_);
                if (existing != nullptr) {
                    had_original_ = true;
                    original_value_ = existing;
                }
            }

            ScopedEnvVar(const ScopedEnvVar&) = delete;
            ScopedEnvVar& operator=(const ScopedEnvVar&) = delete;

            ~ScopedEnvVar() {
                if (had_original_) {
                    setenv(name_, original_value_.c_str(), 1);
                } else {
                    unsetenv(name_);
                }
            }

            void set(const std::string& value) const {
                setenv(name_, value.c_str(), 1);
            }

            void unset() const {
                unsetenv(name_);
            }

        private:
            const char* name_;
            bool had_original_{false};
            std::string original_value_;
        };

        struct SchemaUploadExecutorFixture {
            std::shared_ptr<DatabaseManager> db;
            std::shared_ptr<GqlExecutor> executor;
        };

        [[nodiscard]] auto schema_upload_has_error_code(
            const ExecutionResult& result,
            const EErrorCodes code) -> bool
        {
            return std::any_of(result.errors.begin(), result.errors.end(),
                [code](const auto& error) { return error.code == code; });
        }

        [[nodiscard]] auto make_schema_upload_executor(const std::string& label)
            -> SchemaUploadExecutorFixture
        {
            const auto root = std::filesystem::temp_directory_path()
                / ("isched_schema_exec_" + label + "_"
                   + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
            std::filesystem::create_directories(root / "tenants");

            DatabaseManager::Config cfg;
            cfg.base_path = (root / "tenants").string();
            cfg.system_db_path = (root / "system.sqlite3").string();

            auto db = std::make_shared<DatabaseManager>(cfg);
            REQUIRE(db->ensure_system_db());
            return {db, std::make_shared<GqlExecutor>(db)};
        }

        [[nodiscard]] auto schema_upload_admin_ctx(const std::string& tenant_id)
            -> ResolverCtx
        {
            ResolverCtx ctx;
            ctx.tenant_id = tenant_id;
            ctx.current_user_id = "tenant_admin_test";
            ctx.roles = {"role_tenant_admin"};
            return ctx;
        }

        [[nodiscard]] auto schema_upload_user_ctx(const std::string& tenant_id)
            -> ResolverCtx
        {
            ResolverCtx ctx;
            ctx.tenant_id = tenant_id;
            ctx.current_user_id = "tenant_user_test";
            ctx.roles = {"role_user"};
            return ctx;
        }

        [[nodiscard]] auto schema_upload_upload_mutation() -> std::string
        {
            return R"(
                mutation($input: UploadSchemaDocumentInput!) {
                    uploadSchemaDocument(input: $input) {
                        success
                        schema { name createdAt updatedAt updatedBy }
                        error { code message conflictingName }
                    }
                }
            )";
        }

        [[nodiscard]] auto schema_upload_upload_variables(
            const std::string& name,
            const std::string& content,
            const bool overwrite = false) -> std::string
        {
            return json{
                {"input", {
                    {"name", name},
                    {"content", content},
                    {"overwrite", overwrite}
                }}
            }.dump();
        }

    } // namespace

    TEST_CASE("T041: simple query within limits succeeds", "[gql][executor][T041][complexity]") {
        GqlExecutor::Config cfg;
        cfg.max_depth       = 5;
        cfg.max_complexity  = 20;
        GqlExecutor proc(std::make_shared<DatabaseManager>(), cfg);
        const auto reply = proc.execute("{ hello }");
        REQUIRE(reply.is_success());
    }

    TEST_CASE("T041: query exceeding max_depth is rejected", "[gql][executor][T041][complexity]") {
        GqlExecutor::Config cfg;
        cfg.max_depth = 1;
        GqlExecutor proc(std::make_shared<DatabaseManager>(), cfg);
        proc.register_resolver({}, "a", [](const json&, const json&, const ResolverCtx&) -> json {
            return json{{"b","v"}};
        });
        const auto load_res = proc.load_schema(
            "type Query { a: AType } type AType { b: String }");
        REQUIRE(load_res.is_success());

        const auto reply = proc.execute("{ a { b } }");
        REQUIRE_FALSE(reply.is_success());
        bool found = false;
        for (const auto& e : reply.errors)
            if (e.message.find("depth") != std::string::npos) { found = true; break; }
        REQUIRE(found);
    }

    TEST_CASE("T041: query exceeding max_complexity is rejected", "[gql][executor][T041][complexity]") {
        GqlExecutor::Config cfg;
        cfg.max_complexity = 2;
        GqlExecutor proc(std::make_shared<DatabaseManager>(), cfg);

        const auto reply = proc.execute("{ hello version uptime }");
        REQUIRE_FALSE(reply.is_success());
        bool found = false;
        for (const auto& e : reply.errors)
            if (e.message.find("complexity") != std::string::npos) { found = true; break; }
        REQUIRE(found);
    }

    TEST_CASE("T041: unlimited (0) depth/complexity imposes no restriction", "[gql][executor][T041][complexity]") {
        GqlExecutor proc(std::make_shared<DatabaseManager>());
        const auto reply = proc.execute("{ hello version uptime }");
        REQUIRE(reply.is_success());
    }

    TEST_CASE("Schema upload executor enforces tenant-admin authorization and conflict semantics",
              "[gql][executor][schema-upload][auth][conflict]") {
        auto fixture = make_schema_upload_executor("auth_conflict");
        auto& db = fixture.db;
        auto& exec = fixture.executor;
        REQUIRE(db->initialize_tenant("org_auth"));
        const auto mutation = schema_upload_upload_mutation();

        const auto create_result = exec->execute(
            mutation,
            schema_upload_upload_variables("billing-v1", std::string(k_schema_upload_test_sdl)),
            schema_upload_admin_ctx("org_auth"));
        REQUIRE(create_result.is_success());
        REQUIRE(create_result.data["uploadSchemaDocument"]["success"] == true);
        REQUIRE(create_result.data["uploadSchemaDocument"]["schema"]["name"] == "billing-v1");

        SECTION("regular tenant user is rejected before resolver execution") {
            const auto result = exec->execute(
                mutation,
                schema_upload_upload_variables("billing-v2", std::string(k_schema_upload_test_sdl)),
                schema_upload_user_ctx("org_auth"));
            REQUIRE_FALSE(result.is_success());
            REQUIRE(schema_upload_has_error_code(result, EErrorCodes::FORBIDDEN));
            REQUIRE(result.data["uploadSchemaDocument"].is_null());
        }

        SECTION("duplicate without overwrite returns structured conflict payload") {
            const auto result = exec->execute(
                mutation,
                schema_upload_upload_variables("billing-v1", std::string(k_schema_upload_test_sdl)),
                schema_upload_admin_ctx("org_auth"));
            REQUIRE(result.is_success());
            REQUIRE(result.data["uploadSchemaDocument"]["success"] == false);
            REQUIRE(result.data["uploadSchemaDocument"]["error"]["code"] == "CONFLICT");
            REQUIRE(result.data["uploadSchemaDocument"]["error"]["conflictingName"] == "billing-v1");
        }
    }

    TEST_CASE("Schema upload executor validates names and configurable content size limit",
              "[gql][executor][schema-upload][validation]") {
        ScopedEnvVar schema_upload_max_bytes{"ISCHED_SCHEMA_UPLOAD_MAX_BYTES"};
        auto fixture = make_schema_upload_executor("validation");
        auto& db = fixture.db;
        auto& exec = fixture.executor;
        REQUIRE(db->initialize_tenant("org_validation"));
        const auto ctx = schema_upload_admin_ctx("org_validation");
        const auto mutation = schema_upload_upload_mutation();

        SECTION("invalid document names are rejected") {
            const auto result = exec->execute(
                mutation,
                schema_upload_upload_variables("bad name", std::string(k_schema_upload_test_sdl)),
                ctx);
            REQUIRE(result.is_success());
            REQUIRE(result.data["uploadSchemaDocument"]["success"] == false);
            REQUIRE(result.data["uploadSchemaDocument"]["error"]["code"] == "VALIDATION_FAILED");
        }

        SECTION("default 1 MB limit rejects oversized SDL before parse") {
            schema_upload_max_bytes.unset();
            const auto oversized = std::string((1024U * 1024U) + 1U, 'x');
            const auto result = exec->execute(
                mutation,
                schema_upload_upload_variables("oversized-default", oversized),
                ctx);
            REQUIRE(result.is_success());
            REQUIRE(result.data["uploadSchemaDocument"]["success"] == false);
            REQUIRE(result.data["uploadSchemaDocument"]["error"]["code"] == "VALIDATION_FAILED");
            REQUIRE(result.data["uploadSchemaDocument"]["error"]["message"]
                    .get<std::string>()
                    .find("1048576") != std::string::npos);
        }

        SECTION("environment override changes the active byte cap") {
            schema_upload_max_bytes.set("32");
            const auto accepted = exec->execute(
                mutation,
                schema_upload_upload_variables("small-doc", std::string(k_schema_upload_test_sdl)),
                ctx);
            REQUIRE(accepted.is_success());
            REQUIRE(accepted.data["uploadSchemaDocument"]["success"] == true);

            const auto oversized = std::string(k_schema_upload_test_sdl) + "      ";
            REQUIRE(oversized.size() > 32U);
            const auto rejected = exec->execute(
                mutation,
                schema_upload_upload_variables("too-large", oversized),
                ctx);
            REQUIRE(rejected.is_success());
            REQUIRE(rejected.data["uploadSchemaDocument"]["success"] == false);
            REQUIRE(rejected.data["uploadSchemaDocument"]["error"]["code"] == "VALIDATION_FAILED");
            REQUIRE(rejected.data["uploadSchemaDocument"]["error"]["message"]
                    .get<std::string>()
                    .find("32") != std::string::npos);
        }
    }

    TEST_CASE("Schema document executor queries remain tenant-scoped and null on miss",
              "[gql][executor][schema-upload][read-paths]") {
        auto fixture = make_schema_upload_executor("read_paths");
        auto& db = fixture.db;
        auto& exec = fixture.executor;
        REQUIRE(db->initialize_tenant("org_a"));
        REQUIRE(db->initialize_tenant("org_b"));
        const auto mutation = schema_upload_upload_mutation();

        const auto upload = exec->execute(
            mutation,
            schema_upload_upload_variables("Billing", std::string(k_schema_upload_test_sdl)),
            schema_upload_admin_ctx("org_a"));
        REQUIRE(upload.is_success());
        REQUIRE(upload.data["uploadSchemaDocument"]["success"] == true);

        SECTION("schemaDocuments returns only tenant-local summaries") {
            const auto list_a = exec->execute(
                "query { schemaDocuments { name createdAt updatedAt updatedBy } }",
                "{}",
                schema_upload_user_ctx("org_a"));
            REQUIRE(list_a.is_success());
            REQUIRE(list_a.data["schemaDocuments"].is_array());
            REQUIRE(list_a.data["schemaDocuments"].size() == 1);
            REQUIRE(list_a.data["schemaDocuments"][0]["name"] == "Billing");
            REQUIRE_FALSE(list_a.data["schemaDocuments"][0].contains("sizeBytes"));

            const auto list_b = exec->execute(
                "query { schemaDocuments { name } }",
                "{}",
                schema_upload_user_ctx("org_b"));
            REQUIRE(list_b.is_success());
            REQUIRE(list_b.data["schemaDocuments"].empty());
        }

        SECTION("schemaDocument fetch is exact-name, tenant-scoped, and null on miss") {
            const auto fetch_hit = exec->execute(
                R"(query { schemaDocument(name: "Billing") { name content updatedBy } })",
                "{}",
                schema_upload_user_ctx("org_a"));
            REQUIRE(fetch_hit.is_success());
            REQUIRE(fetch_hit.data["schemaDocument"]["name"] == "Billing");
            REQUIRE(fetch_hit.data["schemaDocument"]["content"] == k_schema_upload_test_sdl);

            const auto fetch_miss_case = exec->execute(
                R"(query { schemaDocument(name: "billing") { name } })",
                "{}",
                schema_upload_user_ctx("org_a"));
            REQUIRE(fetch_miss_case.is_success());
            REQUIRE(fetch_miss_case.data["schemaDocument"].is_null());

            const auto fetch_miss_tenant = exec->execute(
                R"(query { schemaDocument(name: "Billing") { name } })",
                "{}",
                schema_upload_user_ctx("org_b"));
            REQUIRE(fetch_miss_tenant.is_success());
            REQUIRE(fetch_miss_tenant.data["schemaDocument"].is_null());
        }
    }

    TEST_CASE("GraphQL Features and Resolver Complete Coverage", "[graphql][features][coverage]") {
        auto fixture = make_schema_upload_executor("complete_coverage");
        auto& db = fixture.db;
        auto& exec = fixture.executor;

        ResolverCtx admin_ctx;
        admin_ctx.roles = {"role_platform_admin"};

        SECTION("env query verification") {
            ScopedEnvVar env_user("USER");
            env_user.set("test_user");
            ScopedEnvVar env_home("HOME");
            env_home.set("/home/test_user");
            ScopedEnvVar env_path("PATH");
            env_path.set("/usr/bin");
            ScopedEnvVar env_lang("LANG");
            env_lang.set("en_US.UTF-8");
            ScopedEnvVar env_tz("TZ");
            env_tz.set("UTC");

            auto res = exec->execute(R"(
                query {
                    env {
                        systemProperties {
                            os_name
                            user_name
                            user_home
                            file_separator
                            path_separator
                        }
                        environmentVariables {
                            PATH
                            HOME
                            USER
                            LANG
                            TZ
                        }
                    }
                }
            )", "{}", admin_ctx);
            REQUIRE(res.is_success());
            auto env_res = res.data["env"];
            REQUIRE(env_res.is_object());
            REQUIRE(env_res["systemProperties"]["os_name"] == "Linux");
            REQUIRE(env_res["systemProperties"]["user_name"] == "test_user");
            REQUIRE(env_res["systemProperties"]["user_home"] == "/home/test_user");
            REQUIRE(env_res["environmentVariables"]["USER"] == "test_user");
            REQUIRE(env_res["environmentVariables"]["HOME"] == "/home/test_user");
        }

        SECTION("configprops query verification") {
            auto res = exec->execute(R"(
                query {
                    configprops {
                        server {
                            port
                            host
                            maxConnections
                            threadPoolSize
                        }
                        database {
                            type
                            connectionPoolSize
                            enableWAL
                        }
                        features
                        version
                        environment
                    }
                }
            )", "{}", admin_ctx);
            REQUIRE(res.is_success());
            auto config = res.data["configprops"];
            REQUIRE(config["server"]["port"] == 8080);
            REQUIRE(config["server"]["host"] == "0.0.0.0");
            REQUIRE(config["database"]["type"] == "SQLite");
            REQUIRE(config["version"] == "1.0.0");
            REQUIRE(config["environment"] == "development");
        }

        SECTION("Subscription initial state verification") {
            SECTION("healthChanged initial state") {
                auto res = exec->execute("subscription { healthChanged { status timestamp } }");
                REQUIRE(res.is_success());
                REQUIRE(res.data["healthChanged"]["status"] == "UP");
            }

            SECTION("serverMetricsUpdated platform-admin authorized") {
                ResolverCtx actx;
                actx.roles = {"role_platform_admin"};
                auto res = exec->execute("subscription { serverMetricsUpdated { requestsInInterval } }", "{}", actx);
                REQUIRE(res.is_success());
                REQUIRE(res.data["serverMetricsUpdated"].is_object());
            }

            SECTION("serverMetricsUpdated unauthorized") {
                ResolverCtx uctx;
                uctx.roles = {"role_tenant_admin"};
                auto res = exec->execute("subscription { serverMetricsUpdated { requestsInInterval } }", "{}", uctx);
                REQUIRE_FALSE(res.is_success());
                REQUIRE(res.errors[0].code == gql::EErrorCodes::FORBIDDEN);
            }

            SECTION("tenantMetricsUpdated tenant-admin authorized") {
                ResolverCtx actx;
                actx.tenant_id = "tenant_test";
                actx.roles = {"role_tenant_admin"};
                auto res = exec->execute(R"(subscription { tenantMetricsUpdated(organizationId: "tenant_test") { requestsInInterval } })", "{}", actx);
                REQUIRE(res.is_success());
                REQUIRE(res.data["tenantMetricsUpdated"].is_object());
            }

            SECTION("tenantMetricsUpdated platform-admin authorized") {
                ResolverCtx actx;
                actx.tenant_id = "tenant_test";
                actx.roles = {"role_platform_admin"};
                auto res = exec->execute(R"(subscription { tenantMetricsUpdated(organizationId: "tenant_test") { requestsInInterval } })", "{}", actx);
                REQUIRE(res.is_success());
                REQUIRE(res.data["tenantMetricsUpdated"].is_object());
            }

            SECTION("tenantMetricsUpdated unauthorized") {
                ResolverCtx uctx;
                uctx.roles = {"role_user"};
                auto res = exec->execute(R"(subscription { tenantMetricsUpdated(organizationId: "tenant_test") { requestsInInterval } })", "{}", uctx);
                REQUIRE_FALSE(res.is_success());
                REQUIRE(res.errors[0].code == gql::EErrorCodes::FORBIDDEN);
            }

            SECTION("configurationActivated initial state") {
                auto res = exec->execute(R"(subscription { configurationActivated(tenantId: "tenant_test") { tenantId snapshotId } })");
                REQUIRE(res.is_success());
                REQUIRE(res.data["configurationActivated"].is_null());
            }
        }

        SECTION("Error handling and validation paths") {
            SECTION("activateSnapshot database not available") {
                GqlExecutor proc_no_db(nullptr);
                auto res = proc_no_db.execute(R"(mutation { activateSnapshot(id: "some_id") { success errors } })", "{}", admin_ctx);
                REQUIRE(res.is_success());
                REQUIRE(res.data["activateSnapshot"]["success"] == false);
                REQUIRE(res.data["activateSnapshot"]["errors"][0] == "Database not available");
            }

            SECTION("activateSnapshot missing parameter id") {
                auto res = exec->execute(R"(mutation { activateSnapshot(id: 123) { success errors } })", "{}", admin_ctx);
                REQUIRE(res.is_success());
                REQUIRE(res.data["activateSnapshot"]["success"] == false);
                REQUIRE(res.data["activateSnapshot"]["errors"][0].get<std::string>().find("id") != std::string::npos);
            }

            SECTION("activateSnapshot snapshot not found") {
                auto res = exec->execute(R"(mutation { activateSnapshot(id: "nonexistent_snap") { success errors } })", "{}", admin_ctx);
                REQUIRE(res.is_success());
                REQUIRE(res.data["activateSnapshot"]["success"] == false);
                REQUIRE(res.data["activateSnapshot"]["errors"][0] == "Snapshot not found: nonexistent_snap");
            }

            SECTION("rollbackConfiguration database not available") {
                GqlExecutor proc_no_db(nullptr);
                auto res = proc_no_db.execute(R"(mutation { rollbackConfiguration(tenantId: "some_tenant") { success errors } })", "{}", admin_ctx);
                REQUIRE(res.is_success());
                REQUIRE(res.data["rollbackConfiguration"]["success"] == false);
                REQUIRE(res.data["rollbackConfiguration"]["errors"][0] == "Database not available");
            }

            SECTION("rollbackConfiguration missing parameter tenantId") {
                auto res = exec->execute(R"(mutation { rollbackConfiguration(tenantId: 123) { success errors } })", "{}", admin_ctx);
                REQUIRE(res.is_success());
                REQUIRE(res.data["rollbackConfiguration"]["success"] == false);
                REQUIRE(res.data["rollbackConfiguration"]["errors"][0].get<std::string>().find("tenantId") != std::string::npos);
            }

            SECTION("deleteDataSource missing parameter id") {
                auto res = exec->execute(R"(mutation { deleteDataSource(organizationId: "org_a", id: "") })", "{}", admin_ctx);
                REQUIRE_FALSE(res.is_success());
                REQUIRE(res.errors[0].message.find("deleteDataSource: id is required") != std::string::npos);
            }

            SECTION("updateTenantConfig missing parameter organizationId") {
                auto res = exec->execute(R"(mutation { updateTenantConfig(minThreads: 5) { minThreads } })", "{}", admin_ctx);
                REQUIRE_FALSE(res.is_success());
                REQUIRE(res.errors[0].message.find("updateTenantConfig: organizationId is required") != std::string::npos);
            }

            SECTION("updateTenantConfig valid arguments update") {
                REQUIRE(db->initialize_tenant("org_a"));
                auto res = exec->execute(R"(
                    mutation {
                        updateTenantConfig(organizationId: "org_a", minThreads: 5, maxThreads: 10, metricsInterval: 12) {
                            organizationId
                            minThreads
                            maxThreads
                            metricsIntervalMinutes
                        }
                    }
                )", "{}", admin_ctx);
                REQUIRE(res.is_success());
                auto config = res.data["updateTenantConfig"];
                REQUIRE(config["organizationId"] == "org_a");
                REQUIRE(config["minThreads"] == 5);
                REQUIRE(config["maxThreads"] == 10);
                REQUIRE(config["metricsIntervalMinutes"] == 12);
            }

            SECTION("shutdown authorization check") {
                SECTION("shutdown rejected - unauthorized") {
                    ResolverCtx uctx;
                    uctx.roles = {"role_user"};
                    auto res = exec->execute("mutation { shutdown }", "{}", uctx);
                    REQUIRE_FALSE(res.is_success());
                    REQUIRE(res.errors[0].message.find("shutdown: requires platform_admin role") != std::string::npos);
                }

                SECTION("shutdown accepted - platform_admin role") {
                    auto res = exec->execute("mutation { shutdown }", "{}", admin_ctx);
                    REQUIRE(res.is_success());
                    REQUIRE(res.data["shutdown"] == true);
                }

                SECTION("shutdown accepted - token") {
                    ScopedEnvVar env_token("ISCHED_SHUTDOWN_TOKEN");
                    env_token.set("my_secret_token");
                    ResolverCtx tctx;
                    tctx.bearer_token = "my_secret_token";
                    auto res = exec->execute("mutation { shutdown }", "{}", tctx);
                    REQUIRE(res.is_success());
                    REQUIRE(res.data["shutdown"] == true);
                }
            }
        }

        SECTION("Organization and role mutations") {
            SECTION("createOrganization") {
                auto res = exec->execute(R"(
                    mutation {
                        createOrganization(input: {
                            id: "org_testing_id",
                            name: "Org Testing",
                            domain: "testing.org",
                            subscriptionTier: "pro",
                            userLimit: 100,
                            storageLimit: 50000
                        }) {
                            id
                            name
                            domain
                            subscriptionTier
                            userLimit
                            storageLimit
                            status
                            revision
                        }
                    }
                )", "{}", admin_ctx);
                REQUIRE(res.is_success());
                auto org = res.data["createOrganization"];
                REQUIRE(org["id"] == "org_testing_id");
                REQUIRE(org["name"] == "Org Testing");
                REQUIRE(org["domain"] == "testing.org");
                REQUIRE(org["subscriptionTier"] == "pro");
                REQUIRE(org["userLimit"] == 100);
                REQUIRE(org["storageLimit"] == 50000);
                REQUIRE(org["status"] == "ACTIVE");
                REQUIRE(org["revision"] == 0);

                SECTION("createOrganization duplicate error") {
                    auto res2 = exec->execute(R"(
                        mutation {
                            createOrganization(input: {
                                id: "org_testing_id",
                                name: "Org Testing Duplicate"
                            }) {
                                id
                            }
                        }
                    )", "{}", admin_ctx);
                    REQUIRE_FALSE(res2.is_success());
                    REQUIRE(res2.errors[0].message.find("already exists") != std::string::npos);
                }

                SECTION("updateOrganization success") {
                    auto res2 = exec->execute(R"(
                        mutation {
                            updateOrganization(
                                id: "org_testing_id",
                                input: {
                                    name: "Org Updated",
                                    status: "SUSPENDED"
                                }
                            ) {
                                id
                                name
                                status
                                revision
                            }
                        }
                    )", "{}", admin_ctx);
                    REQUIRE(res2.is_success());
                    auto org2 = res2.data["updateOrganization"];
                    REQUIRE(org2["name"] == "Org Updated");
                    REQUIRE(org2["status"] == "SUSPENDED");
                    REQUIRE(org2["revision"] == 1);
                }

                SECTION("deleteOrganization success") {
                    auto res2 = exec->execute(R"(mutation { deleteOrganization(id: "org_testing_id") })", "{}", admin_ctx);
                    REQUIRE(res2.is_success());
                    REQUIRE(res2.data["deleteOrganization"] == true);
                }
            }

            SECTION("updateOrganization not found") {
                auto res = exec->execute(R"(
                    mutation {
                        updateOrganization(
                            id: "nonexistent_org",
                            input: {
                                name: "New Name"
                            }
                        ) {
                            id
                        }
                    }
                )", "{}", admin_ctx);
                REQUIRE_FALSE(res.is_success());
                REQUIRE(res.errors[0].message.find("not found") != std::string::npos);
            }

            SECTION("deleteOrganization not found") {
                auto res = exec->execute(R"(mutation { deleteOrganization(id: "nonexistent_org") })", "{}", admin_ctx);
                REQUIRE_FALSE(res.is_success());
                REQUIRE(res.errors[0].message.find("not found") != std::string::npos);
            }

            SECTION("createRole scope platform") {
                auto res = exec->execute(R"(
                    mutation {
                        createRole(input: {
                            id: "role_custom",
                            name: "Custom Role",
                            description: "My custom role",
                            scope: "platform"
                        })
                    }
                )", "{}", admin_ctx);
                REQUIRE(res.is_success());
                REQUIRE(res.data["createRole"] == true);

                SECTION("createRole duplicate error") {
                    auto res2 = exec->execute(R"(
                        mutation {
                            createRole(input: {
                                id: "role_custom",
                                name: "Custom Role Duplicate",
                                scope: "platform"
                            })
                        }
                    )", "{}", admin_ctx);
                    REQUIRE_FALSE(res2.is_success());
                    REQUIRE(res2.errors[0].message.find("already exists") != std::string::npos);
                }

                SECTION("deleteRole success") {
                    auto res2 = exec->execute(R"(mutation { deleteRole(id: "role_custom") })", "{}", admin_ctx);
                    REQUIRE(res2.is_success());
                    REQUIRE(res2.data["deleteRole"] == true);
                }
            }

            SECTION("createRole validation errors") {
                auto res = exec->execute(R"(
                    mutation {
                        createRole(input: {
                            id: "",
                            name: "Invalid",
                            scope: ""
                        })
                    }
                )", "{}", admin_ctx);
                REQUIRE_FALSE(res.is_success());
                REQUIRE(res.errors[0].message.find("id, name, and scope are required") != std::string::npos);
            }

            SECTION("deleteRole validation errors") {
                auto res = exec->execute(R"(mutation { deleteRole(id: "") })", "{}", admin_ctx);
                REQUIRE_FALSE(res.is_success());
                REQUIRE(res.errors[0].message.find("id is required") != std::string::npos);
            }

            SECTION("deleteRole built-in role deletion error") {
                auto res = exec->execute(R"(mutation { deleteRole(id: "role_platform_admin") })", "{}", admin_ctx);
                REQUIRE_FALSE(res.is_success());
                REQUIRE(res.errors[0].message.find("Built-in roles cannot be deleted") != std::string::npos);
            }

            SECTION("deleteRole role not found error") {
                auto res = exec->execute(R"(mutation { deleteRole(id: "nonexistent_role") })", "{}", admin_ctx);
                REQUIRE_FALSE(res.is_success());
                REQUIRE(res.errors[0].message.find("not found") != std::string::npos);
            }
        }

        SECTION("Health check database DOWN / database null") {
            GqlExecutor proc_no_db(nullptr);
            auto res = proc_no_db.execute("query { health { status components { database { status details { error } } } } }");
            REQUIRE(res.is_success());
            auto h = res.data["health"];
            REQUIRE(h["status"] == "DOWN");
            REQUIRE(h["components"]["database"]["status"] == "DOWN");
        }

        SECTION("tenantMetrics access control and parameter validation") {
            SECTION("tenantMetrics access denied for other organization") {
                ResolverCtx tenant_ctx;
                tenant_ctx.tenant_id = "org_a";
                tenant_ctx.roles = {"role_tenant_admin"};
                auto res = exec->execute(R"(query { tenantMetrics(organizationId: "org_b") { requestsInInterval } })", "{}", tenant_ctx);
                REQUIRE_FALSE(res.is_success());
                REQUIRE_FALSE(res.errors.empty());
                REQUIRE(res.errors[0].code == gql::EErrorCodes::FORBIDDEN);
                REQUIRE(res.errors[0].message.find("Access denied: cannot view metrics for another organization") != std::string::npos);
            }

            SECTION("tenantMetrics organizationId required for empty tenant context") {
                ResolverCtx empty_ctx;
                empty_ctx.roles = {"role_tenant_admin"};
                auto res = exec->execute(R"(query { tenantMetrics { requestsInInterval } })", "{}", empty_ctx);
                REQUIRE_FALSE(res.is_success());
                REQUIRE_FALSE(res.errors.empty());
                REQUIRE(res.errors[0].code == gql::EErrorCodes::VALIDATION_FAILED);
                REQUIRE(res.errors[0].message.find("organizationId is required") != std::string::npos);
            }
        }

        SECTION("serverMetrics and tenantMetrics query fallback when metrics manager is null") {
            SECTION("serverMetrics query") {
                auto res = exec->execute(R"(
                    query {
                        serverMetrics {
                            requestsInInterval
                            errorsInInterval
                            totalRequestsSinceStartup
                            totalErrorsSinceStartup
                            activeConnections
                            activeSubscriptions
                            avgResponseTimeMs
                            tenantCount
                        }
                    }
                )", "{}", admin_ctx);
                REQUIRE(res.is_success());
                auto metrics = res.data["serverMetrics"];
                REQUIRE(metrics["requestsInInterval"] == 0);
                REQUIRE(metrics["tenantCount"] == 0);
            }

            SECTION("tenantMetrics query") {
                auto res = exec->execute(R"(
                    query {
                        tenantMetrics(organizationId: "complete_coverage") {
                            organizationId
                            requestsInInterval
                            errorsInInterval
                            totalRequestsSinceStartup
                            totalErrorsSinceStartup
                            avgResponseTimeMs
                        }
                    }
                )", "{}", admin_ctx);
                REQUIRE(res.is_success());
                auto metrics = res.data["tenantMetrics"];
                REQUIRE(metrics["organizationId"] == "complete_coverage");
                REQUIRE(metrics["requestsInInterval"] == 0);
            }
        }
    }

} // namespace isched::v0_0_1::backend
