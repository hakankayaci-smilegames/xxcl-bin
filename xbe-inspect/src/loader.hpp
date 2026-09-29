#pragma once

#include "xbe.hpp"
#include <string>

namespace xbe {
    bool load_and_run(const ParseResult& res, const std::string& filepath);
}
