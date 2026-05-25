// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

namespace isched::v0_0_1::backend::srvcli {

struct SeedCredentials {
    std::string admin_name;
    std::string password;
};

struct CliOptions {
    std::optional<std::string> data_dir;
    std::optional<SeedCredentials> seed_credentials;
    bool help_requested{false};
};

struct SeedResult {
    std::optional<std::filesystem::path> backup_path;
};

[[nodiscard]] std::optional<SeedCredentials> parse_seed_credentials(std::string_view value,
                                                                    std::string& error);
[[nodiscard]] std::optional<CliOptions> parse_cli_args(int argc, char** argv,
                                                       std::string& error);
[[nodiscard]] std::string build_usage_text(std::string_view program);
[[nodiscard]] bool validate_data_dir(const std::string& path, std::string& error);

bool prompt_seed_confirmation(std::istream& in, std::ostream& out);

[[nodiscard]] std::optional<SeedResult> reset_and_seed_platform_db(const std::string& work_directory,
                                                                   const SeedCredentials& credentials,
                                                                   std::string& error);

} // namespace isched::v0_0_1::backend::srvcli


