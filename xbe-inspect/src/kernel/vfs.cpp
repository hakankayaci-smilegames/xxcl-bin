#include "vfs.hpp"
#include <sys/stat.h>
#include <sys/types.h>
#include <algorithm>
#include <cctype>
#include <iostream>

namespace xbe {
namespace kernel {

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

} // namespace kernel
} // namespace xbe
