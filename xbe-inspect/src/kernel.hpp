#pragma once

#include "xbe.hpp"
#include <cstdint>

namespace xbe {
    void resolve_kernel_imports(const ParseResult& res, uintptr_t delta);
    
    // Debugging & Telemetry
    void start_execution_timer();
    void record_kernel_call(uint32_t ordinal);
    void print_execution_stats();
}
