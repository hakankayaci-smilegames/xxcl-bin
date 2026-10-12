#include "vfs.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <utility>

namespace xbe {
namespace kernel {

namespace fs = std::filesystem;

static std::map<std::string, std::string> g_symlinks;
static fs::path g_host_root = "./hdd";

static std::string normalize_separators(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

static bool has_path_prefix(const std::string& path, const std::string& prefix) {
    if (path.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(path[i])) !=
            std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return path.size() == prefix.size() || path[prefix.size()] == '/';
}

static std::string strip_leading_slashes(std::string path) {
    while (!path.empty() && path.front() == '/') path.erase(path.begin());
    return path;
}

static bool is_safe_relative_path(const std::string& path) {
    for (const auto& component : fs::path(path)) {
        if (component == "..") return false;
    }
    return true;
}

static std::string resolve_symbolic_links(std::string path) {
    path = normalize_separators(std::move(path));
    for (size_t depth = 0; depth < 16; ++depth) {
        bool resolved = false;
        for (const auto& [link, target] : g_symlinks) {
            const std::string normalized_link = normalize_separators(link);
            if (!has_path_prefix(path, normalized_link)) continue;
            path = normalize_separators(target) + path.substr(normalized_link.size());
            resolved = true;
            break;
        }
        if (!resolved) break;
    }
    return path;
}

void vfs_init() {
    fs::create_directories(g_host_root / "E" / "UDATA");
    fs::create_directories(g_host_root / "E" / "TDATA");
    fs::create_directories(g_host_root / "D");
    fs::create_directories(g_host_root / "C");
}

void vfs_set_host_root(const std::string& root) {
    g_host_root = root.empty() ? fs::path("./hdd") : fs::path(root);
}

std::string vfs_translate_path(const std::string& xbox_path) {
    const std::string path = resolve_symbolic_links(xbox_path);

    const std::string lower_path = [&path] {
        std::string lower = path;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return lower;
    }();

    struct DriveMapping {
        const char* device_path;
        char drive;
    };
    static const DriveMapping mappings[] = {
        {"/device/harddisk0/partition1", 'E'},
        {"/device/harddisk0/partition2", 'C'},
        {"/device/cdrom0", 'D'}
    };

    for (const auto& mapping : mappings) {
        const std::string prefix = mapping.device_path;
        if (has_path_prefix(lower_path, prefix)) {
            const std::string relative = strip_leading_slashes(path.substr(prefix.size()));
            if (!is_safe_relative_path(relative)) return "";
            return (g_host_root / std::string(1, mapping.drive) / fs::path(relative)).lexically_normal().string();
        }
    }

    if (path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':' &&
        (path.size() == 2 || path[2] == '/')) {
        const char drive = static_cast<char>(std::toupper(static_cast<unsigned char>(path[0])));
        if (drive == 'C' || drive == 'D' || drive == 'E') {
            const std::string relative = strip_leading_slashes(path.substr(2));
            if (!is_safe_relative_path(relative)) return "";
            return (g_host_root / std::string(1, drive) / fs::path(relative)).lexically_normal().string();
        }
    }

    return "";
}

bool vfs_is_cdrom_path(const std::string& xbox_path) {
    const std::string path = resolve_symbolic_links(xbox_path);
    std::string lower_path = path;
    std::transform(lower_path.begin(), lower_path.end(), lower_path.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return has_path_prefix(lower_path, "/device/cdrom0") ||
           (lower_path.size() >= 2 && lower_path[0] == 'd' && lower_path[1] == ':' &&
            (lower_path.size() == 2 || lower_path[2] == '/'));
}

void vfs_register_symlink(const std::string& link, const std::string& target) {
    g_symlinks[normalize_separators(link)] = normalize_separators(target);
}

void vfs_delete_symlink(const std::string& link) {
    auto it = g_symlinks.find(normalize_separators(link));
    if (it != g_symlinks.end()) {
        g_symlinks.erase(it);
    }
}

} // namespace kernel
} // namespace xbe
