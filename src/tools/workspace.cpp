#include "tools/workspace.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

bool Workspace::resolve(std::string_view user_path, fs::path* out, std::string* reason) const {
    auto reject = [&](const char* why) {
        if (reason) *reason = why;
        return false;
    };

    try {
        // 1. empty
        if (user_path.empty()) return reject("empty path");

        // 2. embedded NUL
        if (user_path.find('\0') != std::string_view::npos)
            return reject("path contains a NUL byte");

        // 3. absolute / rooted paths are never allowed
        fs::path p{std::string(user_path)};
        if (p.is_absolute() || p.has_root_path())
            return reject("absolute paths are not allowed; use a workspace-relative path");

        // 4. purely lexical normalization (no filesystem access)
        fs::path n = p.lexically_normal();

        // 5. a leading ".." means it climbs above the workspace root
        if (n.empty() || *n.begin() == "..")
            return reject("path escapes the workspace");

        // 6. build the final path
        if (n == ".") *out = root();
        else          *out = root() / n;
        return true;
    } catch (...) {
        return reject("invalid path");
    }
}
