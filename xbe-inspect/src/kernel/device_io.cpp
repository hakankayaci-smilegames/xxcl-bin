#include "device_io.hpp"

#include <cstdint>
#include <cstring>

namespace xbe {
namespace kernel {

namespace {
constexpr NTSTATUS STATUS_SUCCESS = 0;
constexpr NTSTATUS STATUS_INVALID_PARAMETER = static_cast<NTSTATUS>(0xC000000D);
constexpr NTSTATUS STATUS_NOT_SUPPORTED = static_cast<NTSTATUS>(0xC00000BB);
constexpr NTSTATUS STATUS_INFO_LENGTH_MISMATCH = static_cast<NTSTATUS>(0xC0000004);
constexpr uint32_t IOCTL_SCSI_PASS_THROUGH_DIRECT = 0x0004D014;

NTSTATUS complete(IO_STATUS_BLOCK* iosb, NTSTATUS status, uintptr_t information = 0) {
    if (iosb) {
        iosb->Status = static_cast<uint32_t>(status);
        iosb->Information = information;
    }
    return status;
}

NTSTATUS cdrom_device_control(
    uint32_t control_code,
    void* input_buffer,
    uint32_t input_length,
    IO_STATUS_BLOCK* iosb
) {
    if (control_code != IOCTL_SCSI_PASS_THROUGH_DIRECT) return STATUS_NOT_SUPPORTED;
    if (!input_buffer || input_length < 32) return STATUS_INVALID_PARAMETER;

    auto* request = static_cast<uint8_t*>(input_buffer);
    uint32_t transfer_length = 0;
    uint32_t data_buffer_address = 0;
    std::memcpy(&transfer_length, request + 12, sizeof(transfer_length));
    std::memcpy(&data_buffer_address, request + 20, sizeof(data_buffer_address));

    const uint8_t opcode = request[28];
    const uint8_t page_code = request[30] & 0x3F;
    if (opcode != 0x5A || page_code != 0x3E) return STATUS_NOT_SUPPORTED;
    if (!data_buffer_address || transfer_length < 28) return STATUS_INFO_LENGTH_MISMATCH;

    auto* data = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(data_buffer_address));
    std::memset(data, 0, 28);
    data[1] = 26;
    data[8] = 0x3E;
    data[9] = 0x12;
    data[10] = 0x01; // Xbox optical media authentication is valid.
    data[11] = 0x01;
    data[12] = 0x0D; // Xbox media book type.
    data[14] = 0x01;
    request[2] = 0; // SCSI status GOOD.
    return complete(iosb, STATUS_SUCCESS, 28);
}
} // namespace

NTSTATUS xbox_device_control_dispatch(
    bool cdrom_device,
    uint32_t control_code,
    void* input_buffer,
    uint32_t input_length,
    void* output_buffer,
    uint32_t output_length,
    IO_STATUS_BLOCK* io_status_block
) {
    (void)output_buffer;
    (void)output_length;
    if (!cdrom_device) return complete(io_status_block, STATUS_NOT_SUPPORTED);
    const NTSTATUS status = cdrom_device_control(control_code, input_buffer, input_length, io_status_block);
    if (status != STATUS_SUCCESS && io_status_block) {
        io_status_block->Status = static_cast<uint32_t>(status);
        io_status_block->Information = 0;
    }
    return status;
}

} // namespace kernel
} // namespace xbe
