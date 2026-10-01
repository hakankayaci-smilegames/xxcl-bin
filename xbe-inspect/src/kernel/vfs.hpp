#pragma once
#include <string>

namespace xbe {
namespace kernel {

void vfs_init();
std::string vfs_translate_path(const std::string& xbox_path);

} // namespace kernel
} // namespace xbe
