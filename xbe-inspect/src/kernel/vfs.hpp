#pragma once
#include <string>

namespace xbe {
namespace kernel {

void vfs_init();
std::string vfs_translate_path(const std::string& xbox_path);
void vfs_register_symlink(const std::string& link, const std::string& target);
void vfs_delete_symlink(const std::string& link);

} // namespace kernel
} // namespace xbe
