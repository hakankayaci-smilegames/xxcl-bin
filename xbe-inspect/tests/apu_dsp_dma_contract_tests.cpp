#include "../src/kernel/apu_dsp_dma.hpp"

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
    return 0;
}
