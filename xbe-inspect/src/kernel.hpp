#pragma once

#include "xbe.hpp"
#include <cstdint>

namespace xbe {
    void resolve_kernel_imports(const ParseResult& res, uintptr_t delta);
    void generic_stub(int ordinal);
}
