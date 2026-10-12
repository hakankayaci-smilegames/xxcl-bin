#include "../src/kernel/apu_dsp_dma.hpp"

#include <array>
#include <cstddef>

namespace {
struct ScratchState {
    bool called = false;
};

bool scratch_transfer(void* context, uint32_t buffer_id, uint32_t offset,
                     uint32_t ring_size, std::span<std::byte> bytes,
                     bool write_to_buffer) {
    auto& state = *static_cast<ScratchState*>(context);
    state.called = true;
    if (buffer_id != 0xF || offset != 0x800 || ring_size != 1 ||
        write_to_buffer || bytes.size() != 24) return false;
    for (size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::byte>(index);
    }
    return true;
}
}

int main() {
    using xbe::kernel::ApuDspDma;
    ApuDspDma dma;

    if (dma.read(ApuDspDma::Register::control) != 0) return 1;
    dma.write(ApuDspDma::Register::control, ApuDspDma::action_freeze);
    if ((dma.read(ApuDspDma::Register::control) & ApuDspDma::control_frozen) == 0) return 2;
    if ((dma.read(ApuDspDma::Register::control) & ApuDspDma::control_running) != 0) return 3;

    dma.write(ApuDspDma::Register::control, ApuDspDma::action_unfreeze);
    if ((dma.read(ApuDspDma::Register::control) & ApuDspDma::control_frozen) != 0) return 4;
    dma.write(ApuDspDma::Register::control, ApuDspDma::action_start);
    if ((dma.read(ApuDspDma::Register::control) & ApuDspDma::control_running) == 0) return 5;
    if ((dma.read(ApuDspDma::Register::control) & ApuDspDma::control_running) == 0) return 6;
    const uint32_t completed = dma.read(ApuDspDma::Register::control);
    if ((completed & ApuDspDma::control_running) != 0 ||
        (completed & ApuDspDma::control_stopped) == 0) return 7;

    dma.write(ApuDspDma::Register::configuration, 0x123456);
    dma.write(ApuDspDma::Register::start_block, 0xABCDEF);
    dma.write(ApuDspDma::Register::next_block, 0xFEDCBA);
    if (dma.read(ApuDspDma::Register::configuration) != 0x123456 ||
        dma.read(ApuDspDma::Register::start_block) != 0xABCDEF ||
        dma.read(ApuDspDma::Register::next_block) != 0xFEDCBA) return 8;

    dma.reset();
    if (dma.read(ApuDspDma::Register::control) != 0 ||
        dma.read(ApuDspDma::Register::configuration) != 0 ||
        dma.read(ApuDspDma::Register::start_block) != 0 ||
        dma.read(ApuDspDma::Register::next_block) != 0) return 9;

    // Walk a terminal X-space descriptor and transfer six 24-bit DSP words
    // from the generic scratch-buffer callback before reporting EOL.
    std::array<uint32_t, 0x1800> x_memory{};
    std::array<uint32_t, 0x800> y_memory{};
    std::array<uint32_t, 0x1000> program_memory{};
    x_memory[6] = 0x4006;
    x_memory[7] = 0x59E0; // scratch buffer 0xF, format 6
    x_memory[8] = 6;
    x_memory[9] = 0;
    x_memory[10] = 0x800;
    x_memory[11] = 0;
    x_memory[12] = 0;
    dma.write(ApuDspDma::Register::next_block, 6);
    dma.write(ApuDspDma::Register::control, ApuDspDma::action_start);
    ScratchState scratch;
    if (!dma.run_descriptors(x_memory, y_memory, program_memory,
                             scratch_transfer, &scratch) || !scratch.called ||
        !dma.end_of_list() || dma.read(ApuDspDma::Register::next_block) != 0x4006 ||
        x_memory[0] != 0x020100 || x_memory[1] != 0x060504 ||
        x_memory[5] != 0x161514) return 10;
    return 0;
}
