#include "../src/kernel/apu_dsp.hpp"
#include "../src/kernel/physical_memory.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

int main() {
    constexpr uint32_t table_physical = 0x00970000;
    constexpr uint32_t scratch_physical = 0x00971000;
    void* table = nullptr;
    void* scratch = nullptr;
    if (posix_memalign(&table, 0x1000, 0x1000) != 0 ||
        posix_memalign(&scratch, 0x1000, 0x2000) != 0) return 1;
    std::memset(table, 0, 0x1000);
    std::memset(scratch, 0, 0x2000);
    if (xbe::kernel::physical_memory_register(table, 0x1000, 0x1000,
                                               table_physical, 0x00970FFF) != table_physical ||
        xbe::kernel::physical_memory_register(scratch, 0x2000, 0x1000,
                                               scratch_physical, 0x00972FFF) != scratch_physical) return 1;

    auto* sge = static_cast<uint32_t*>(table);
    sge[0] = scratch_physical;
    sge[2] = scratch_physical + 0x1000;
    auto* words = static_cast<uint32_t*>(scratch);
    words[0] = 0xAB123456;
    words[0x7FF] = 0xFFABCDEF;

    xbe::kernel::ApuDspCore core;
    if (!core.bootstrap(table_physical, 1)) return 1;
    if (core.program_word(0) != 0x00123456 ||
        core.program_word(0x7FF) != 0x00ABCDEF ||
        core.program_word(0x800) != 0) return 1;

    // The bootstrap must reject a missing second page before changing P memory.
    if (core.bootstrap(table_physical, 0)) return 1;
    if (core.program_word(0) != 0x00123456 || core.program_word(0x7FF) != 0x00ABCDEF) return 1;

    if (core.program_word(xbe::kernel::ApuDspCore::program_word_count) != 0) return 1;

    // The observed GP firmware starts with JSR (absolute), then returns from
    // its subroutine with RTS. Exercise the architectural call/return contract.
    words[0] = 0x000BF080;
    words[1] = 0x00000155;
    words[2] = 0x00300600; // parallel MOVE #$06, R0
    words[3] = 0x0006C420; // REP X0
    words[4] = 0x00565800; // parallel MOVE A, X:(R0)+
    words[5] = 0x00200013; // parallel CLR A
    words[6] = 0x0047F400; // parallel MOVE #$000006, Y1
    words[7] = 0x00000006;
    words[8] = 0x00220E00; // parallel MOVE Y1, A
    words[9] = 0x000140C6; // AND #$00FF00, A
    words[10] = 0x0000FF00;
    words[11] = 0x000140C2; // OR #$00000F, A
    words[12] = 0x0000000F;
    words[13] = 0x0002008E; // MOVE A, X:(R0+$00)
    words[14] = 0x000A708E; // MOVE A, X:(R0+$000004)
    words[15] = 0x00000004;
    words[16] = 0x000140C0; // ADD #$000002, A
    words[17] = 0x00000002;
    words[0x155] = 0x0008F485; // MOVEP #$FFF, X:$C5
    words[0x156] = 0x00000FFF;
    words[0x157] = 0x0060F400; // parallel MOVE #$000000, R0
    words[0x158] = 0x00000000;
    words[0x159] = 0x0044F400; // parallel MOVE #$001000, X0
    words[0x15A] = 0x00001000;
    words[0x15B] = 0x0000000C;
    if (!core.bootstrap(table_physical, 1)) return 1;
    if (core.program_counter() != 0) return 1;
    if (core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        core.program_counter() != 0x155) return 1;
    if (core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        core.program_counter() != 0x157 || core.peripheral_word(false, 5) != 0xFFF) return 1;
    if (core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        core.program_counter() != 0x159) return 1;
    if (core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        core.program_counter() != 0x15B || core.register_word(4) != 0x1000) return 1;
    if (core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        core.program_counter() != 2) return 1;
    if (core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        core.program_counter() != 3 || core.register_word(16) != 6) return 1;
    if (core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        core.program_counter() != 4) return 1;
    for (uint32_t iteration = 0; iteration < 0x1000; ++iteration) {
        if (core.step() != xbe::kernel::ApuDspCore::StepResult::executed) return 1;
        if (iteration + 1 < 0x1000 && core.program_counter() != 4) return 1;
    }
    if (core.program_counter() != 5 || core.register_word(16) != 0x1006 ||
        core.data_word(false, 6) != 0) return 1;
    if (core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        core.program_counter() != 6 || core.register_word(57) != 0x14) return 1;
    if (core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        core.program_counter() != 8 || core.register_word(7) != 6) return 1;
    if (core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        core.program_counter() != 9 || core.register_word(12) != 0x1006) return 1;
    if (core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        core.program_counter() != 11 || core.register_word(12) != 0x1000) return 1;
    if (core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        core.program_counter() != 13 || core.register_word(12) != 0x100F) return 1;
    if (core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        core.program_counter() != 14 || core.data_word(false, 0x1006) != 0x100F) return 1;
    if (core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        core.program_counter() != 16 || core.data_word(false, 0x100A) != 0x100F) return 1;
    if (core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        core.program_counter() != 18 || core.register_word(12) != 0x100F ||
        core.register_word(8) != 2) return 1;

    // DOR sets up a counted hardware loop around the following DSP word.
    words[0] = 0x000BF080;
    words[1] = 0x00000010;
    words[0x10] = 0x00060290; // DOR #2, P[0x12]
    words[0x11] = 0x00000002;
    words[0x12] = 0x00565800; // MOVE A, X:(R0)+
    words[0x13] = 0x0000000C;
    xbe::kernel::ApuDspCore loop_core;
    if (!loop_core.bootstrap(table_physical, 1)) return 1;
    if (loop_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        loop_core.program_counter() != 0x10) return 1;
    if (loop_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        loop_core.program_counter() != 0x12) return 1;
    if (loop_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        loop_core.program_counter() != 0x12 || loop_core.register_word(16) != 1) return 1;
    if (loop_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        loop_core.program_counter() != 0x13 || loop_core.register_word(16) != 2) return 1;
    if (loop_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        loop_core.program_counter() != 2) return 1;

    words[0] = 0x00321400; // parallel MOVE #$14, R2
    words[1] = 0x0007DA84; // MOVE P:(R2)+, X0
    words[20] = 0x00123456;
    xbe::kernel::ApuDspCore movem_core;
    if (!movem_core.bootstrap(table_physical, 1)) return 1;
    if (movem_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        movem_core.program_counter() != 1 || movem_core.register_word(18) != 20) return 1;
    if (movem_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        movem_core.program_counter() != 2 || movem_core.register_word(4) != 0x123456 ||
        movem_core.register_word(18) != 21) return 1;

    words[0] = 0x000D1080; // BSR xxxx
    words[1] = 0x00000040;
    words[0x40] = 0x0000000C; // RTS
    xbe::kernel::ApuDspCore bsr_core;
    if (!bsr_core.bootstrap(table_physical, 1)) return 1;
    if (bsr_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        bsr_core.program_counter() != 0x40) return 1;
    if (bsr_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        bsr_core.program_counter() != 2) return 1;

    words[0] = 0x0008F485; // MOVEP #$000001, X:$C5
    words[1] = 0x00000001;
    words[2] = 0x000CC520; // BRSET #0, X:$05, P[+4]
    words[3] = 0x00000004;
    xbe::kernel::ApuDspCore branch_core;
    if (!branch_core.bootstrap(table_physical, 1)) return 1;
    if (branch_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        branch_core.program_counter() != 2) return 1;
    if (branch_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        branch_core.program_counter() != 6) return 1;

    xbe::kernel::physical_memory_unregister(scratch);
    xbe::kernel::physical_memory_unregister(table);
    std::free(scratch);
    std::free(table);
    return 0;
}
