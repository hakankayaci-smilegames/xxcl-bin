#pragma once
#include <string>

namespace xbe {
namespace kernel {

void vfs_init();
void vfs_set_host_root(const std::string& root);
std::string vfs_translate_path(const std::string& xbox_path);
bool vfs_is_cdrom_path(const std::string& xbox_path);
void vfs_register_symlink(const std::string& link, const std::string& target);
void vfs_delete_symlink(const std::string& link);

} // namespace kernel
} // namespace xbe
