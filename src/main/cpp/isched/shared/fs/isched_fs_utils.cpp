// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_fs_utils.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Implementation of Unix-style glob and filesystem helper utilities.
 */

#include "isched_fs_utils.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace isched::v0_0_1::fsutils {

static std::vector<std::string> split_segments(const fs::path &p) {
    std::string const s = p.generic_string(); // use '/' as separator
    std::vector<std::string> segs;
    std::string cur;
    for (char ch : s) {
        if (ch == '/') {
            if (!cur.empty()) segs.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    if (!cur.empty()) segs.push_back(cur);
    return segs;
}

static bool match_char_class(const std::string &pat, size_t &i, char ch) {
    // pat[i] should be '[' when called. Consume until closing ']'.
    bool neg = false;
    if (i + 1 < pat.size() && (pat[i + 1] == '!' || pat[i + 1] == '^')) {
        neg = true;
        ++i; // move to '!'
    }
    bool ok = false;
    bool closed = false;
    for (++i; i < pat.size(); ++i) { // start after '[' or '[!'
        if (pat[i] == ']') { closed = true; break; }
        if (i + 2 < pat.size() && pat[i + 1] == '-' && pat[i + 2] != ']') {
            char a = pat[i];
            char b = pat[i + 2];
            if (a > b) std::swap(a, b);
            if (ch >= a && ch <= b) ok = true;
            i += 2;
        } else {
            if (pat[i] == ch) ok = true;
        }
    }
    if (!closed) {
        // malformed class, treat '[' literally by rewinding to '[' and compare
        // Find previous '['
        // Not perfect, but safe fallback: literal compare
        return ch == '[';
    }
    return neg ? !ok : ok;
}

static bool wildcard_match_segment(const std::string &pattern, const std::string &name) {
    // Simple glob for a single path segment: supports '*', '?', and character classes.
    size_t pi = 0, ni = 0;
    size_t star = std::string::npos, match = 0;
    while (ni < name.size()) {
        if (pi < pattern.size() && pattern[pi] == '*') {
            star = ++pi; // position after '*'
            match = ni;
        } else if (pi < pattern.size() && pattern[pi] == '?') {
            ++pi; ++ni;
        } else if (pi < pattern.size() && pattern[pi] == '[') {
            if (match_char_class(pattern, pi, name[ni])) { ++ni; ++pi; }
            else if (star != std::string::npos) { pi = star; ++match; ni = match; }
            else return false;
        } else if (pi < pattern.size() && pattern[pi] == '\\') {
            // escape next character
            ++pi;
            if (pi < pattern.size() && pattern[pi] == name[ni]) { ++pi; ++ni; }
            else if (star != std::string::npos) { pi = star; ++match; ni = match; }
            else return false;
        } else if (pi < pattern.size() && pattern[pi] == name[ni]) {
            ++pi; ++ni;
        } else if (star != std::string::npos) {
            pi = star;
            ++match; ni = match;
        } else {
            return false;
        }
    }
    // consume remaining '*' in pattern
    while (pi < pattern.size() && pattern[pi] == '*') ++pi;
    return pi == pattern.size();
}

static void collect(const fs::path &dir,
                    const std::vector<std::string> &segs,
                    size_t idx,
                    std::vector<fs::path> &out) {
    if (idx == segs.size()) {
        if (fs::exists(dir)) out.push_back(fs::weakly_canonical(dir));
        return;
    }

    const std::string &seg = segs[idx];

    if (seg == ".") {
        collect(dir, segs, idx + 1, out);
        return;
    }

    if (seg == "..") {
        collect(dir.parent_path(), segs, idx + 1, out);
        return;
    }

    if (seg == "**") {
        // Option 1: match zero segment
        collect(dir, segs, idx + 1, out);
        // Option 2: recursively traverse
        if (fs::exists(dir) && fs::is_directory(dir)) {
            for (auto const &entry : fs::directory_iterator(dir)) {
                if (fs::is_directory(entry.path())) {
                    collect(entry.path(), segs, idx, out); // keep '**' at same idx
                }
            }
        }
        return;
    }

    if (!fs::exists(dir) || !fs::is_directory(dir)) return;

    for (auto const &entry : fs::directory_iterator(dir)) {
        if (wildcard_match_segment(seg, entry.path().filename().string())) {
            collect(entry.path(), segs, idx + 1, out);
        }
    }
}

std::vector<fs::path> glob(const fs::path &pattern) {
    std::vector<std::string> segs = split_segments(pattern);
    fs::path start = pattern.is_absolute() ? fs::path{"/"} : fs::current_path();
#ifdef _WIN32
    if (pattern.is_absolute()) {
        start = pattern.root_path();
    }
#endif
    std::vector<fs::path> out;
    if (segs.empty()) {
        if (fs::exists(pattern)) out.push_back(fs::weakly_canonical(pattern));
        return out;
    }

    // Collapse multiple consecutive '**' into single for efficiency
    std::vector<std::string> norm;
    for (size_t i = 0; i < segs.size(); ++i) {
        if (!norm.empty() && norm.back() == "**" && segs[i] == "**") continue;
        norm.push_back(segs[i]);
    }
    collect(start, norm, 0, out);

    // Deduplicate and sort
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

    std::string read_file(const fs::path &path) {
        const std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file) {
            throw std::runtime_error("Failed to open file: " + path.string());
        }
        std::ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

} // namespace isched::v0_0_1::fsutils
