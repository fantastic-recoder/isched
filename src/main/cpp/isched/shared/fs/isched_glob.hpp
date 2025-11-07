#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace isched::v0_0_1::fsutils {

/**
 * Unix-style glob using std::filesystem.
 *
 * Supported patterns in path segments:
 *  - *  : matches any sequence of characters (except path separator)
 *  - ?  : matches exactly one character (except path separator)
 *  - [..]: character classes and ranges, e.g. [abc], [a-z], with negation [!a-z]
 *  - ** : special segment that matches zero or more directories (recursive)
 *
 * Notes:
 *  - Hidden files (dotfiles) are not treated specially; they match like any other name.
 *  - Path separators are directory-specific. Pattern separators can be '/' or native.
 */
[[nodiscard]] std::vector<std::filesystem::path> glob(const std::filesystem::path &pattern);

} // namespace isched::v0_0_1::fsutils
