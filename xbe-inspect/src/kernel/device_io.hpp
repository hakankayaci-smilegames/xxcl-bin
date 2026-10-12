#pragma once

#include "io.hpp"

namespace xbe {
namespace kernel {

NTSTATUS xbox_device_control_dispatch(
    bool cdrom_device,
    uint32_t control_code,
    void* input_buffer,
    uint32_t input_length,
    void* output_buffer,
    uint32_t output_length,
    IO_STATUS_BLOCK* io_status_block
);

} // namespace kernel
} // namespace xbe
