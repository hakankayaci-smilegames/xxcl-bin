#include "vfs.hpp"
#include <sys/stat.h>
#include <sys/types.h>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <cstdlib>

namespace xbe {
namespace kernel {

static std::map<std::string, std::string> g_symlinks;

void vfs_init() {
    mkdir("./hdd", 0777);
    mkdir("./hdd/E", 0777);
    mkdir("./hdd/D", 0777);
    mkdir("./hdd/C", 0777);
    mkdir("./hdd/E/UDATA", 0777);
    mkdir("./hdd/E/TDATA", 0777);
}

std::string to_lower(const std::string& s) {
    std::string res = s;
    std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c){ return std::tolower(c); });
    return res;
}

std::string vfs_translate_path(const std::string& xbox_path) {
    std::string p = xbox_path;
    std::replace(p.begin(), p.end(), '\\', '/');
    std::string lower_p = to_lower(p);
    
    for (const auto& pair : g_symlinks) {
        std::string link_lower = to_lower(pair.first);
        std::replace(link_lower.begin(), link_lower.end(), '\\', '/');
        
        if (lower_p.find(link_lower) == 0) {
            std::string remaining = p.substr(link_lower.length());
            std::string new_xbox_path = pair.second + remaining;
            return vfs_translate_path(new_xbox_path);
        }
    }
    
    std::string E_drive = "/device/harddisk0/partition1";
    std::string D_drive = "/device/cdrom0";
    std::string C_drive = "/device/harddisk0/partition2";
    
    if (lower_p.find(E_drive) == 0) {
        return "./hdd/E" + p.substr(E_drive.length());
    } else if (lower_p.find(D_drive) == 0) {
        return "./hdd/D" + p.substr(D_drive.length());
    } else if (lower_p.find(C_drive) == 0) {
        return "./hdd/C" + p.substr(C_drive.length());
    }
    return p;
}

void vfs_register_symlink(const std::string& link, const std::string& target) {
    g_symlinks[link] = target;
    std::string linux_target = vfs_translate_path(target);
    std::string cmd = "mkdir -p '" + linux_target + "'";
    system(cmd.c_str());
}

void vfs_delete_symlink(const std::string& link) {
    auto it = g_symlinks.find(link);
    if (it != g_symlinks.end()) {
        g_symlinks.erase(it);
    }
}

} // namespace kernel
} // namespace xbe
