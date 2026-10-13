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

    // DSP56300 NOP is a one-word instruction: it advances PC and preserves SR.
    words[0] = 0x00200013; // CLR A -> SR=0x14
    words[1] = 0x00000000; // NOP
    if (!core.bootstrap(table_physical, 1) ||
        core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        core.program_counter() != 1 || core.register_word(57) != 0x14 ||
        core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        core.program_counter() != 2 || core.register_word(57) != 0x14) return 28;

    // CMPU X0,A computes the unsigned comparison and updates CCR without storing A.
    words[0] = 0x0044F400; // MOVE #$001000, X0
    words[1] = 0x00001000;
    words[2] = 0x000C1FF8; // CMPU X0,A
    if (!core.bootstrap(table_physical, 1) ||
        core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        core.program_counter() != 3 || core.register_word(4) != 0x1000 ||
        (core.register_word(57) & 0x0D) != 0x09 || core.register_word(8) != 0) return 29;

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
        core.program_counter() != 0x157 || core.peripheral_word(false, 5) != 0) return 1;
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

    // PM5 direct X-memory moves transfer between an absolute data address and
    // a DSP register. The game now reaches this form after DMA EOL.
    words[0] = 0x0044F400; // MOVE #$123456, X0
    words[1] = 0x00123456;
    words[2] = 0x00440000; // MOVE X0, X:$00
    words[3] = 0x0044F400; // MOVE #$000000, X0
    words[4] = 0x00000000;
    words[5] = 0x00448000; // MOVE X:$00, X0
    xbe::kernel::ApuDspCore direct_move_core;
    if (!direct_move_core.bootstrap(table_physical, 1)) return 20;
    for (uint32_t step = 0; step < 4; ++step) {
        if (direct_move_core.step() != xbe::kernel::ApuDspCore::StepResult::executed) return 21;
    }
    if (direct_move_core.program_counter() != 6) return 22;
    if (direct_move_core.register_word(4) != 0x123456) return 23;
    if (direct_move_core.data_word(false, 0) != 0x123456) return 24;

    // PM2 opcode 0x45 compares signed X0 against accumulator A and sets Z
    // without changing either operand.
    words[0] = 0x00200045; // CMP X0,A with parallel R0,R0
    xbe::kernel::ApuDspCore compare_core;
    if (!compare_core.bootstrap(table_physical, 1) ||
        compare_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        compare_core.program_counter() != 1 ||
        (compare_core.register_word(57) & (1u << 2)) == 0) return 25;

    // BEQ consumes its long relative displacement and leaves the condition
    // register unchanged whether the branch is taken or not.
    words[0] = 0x00200013; // CLR A -> Z=1
    words[1] = 0x000D104A; // BEQ
    words[2] = 0x00000039;
    xbe::kernel::ApuDspCore beq_taken_core;
    if (!beq_taken_core.bootstrap(table_physical, 1) ||
        beq_taken_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        beq_taken_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        beq_taken_core.program_counter() != 0x3A ||
        (beq_taken_core.register_word(57) & (1u << 2)) == 0) return 26;

    words[0] = 0x000140C0; // ADD #$000001, A -> Z=0
    words[1] = 0x00000001;
    words[2] = 0x000D104A; // BEQ
    words[3] = 0x00000039;
    xbe::kernel::ApuDspCore beq_not_taken_core;
    if (!beq_not_taken_core.bootstrap(table_physical, 1) ||
        beq_not_taken_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        beq_not_taken_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        beq_not_taken_core.program_counter() != 4 ||
        (beq_not_taken_core.register_word(57) & (1u << 2)) != 0) return 27;

    words[0] = 0x000D1080; // BSR xxxx
    words[1] = 0x00000040;
    words[0x40] = 0x0000000C; // RTS
    xbe::kernel::ApuDspCore bsr_core;
    if (!bsr_core.bootstrap(table_physical, 1)) return 1;
    if (bsr_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        bsr_core.program_counter() != 0x40) return 1;
    if (bsr_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        bsr_core.program_counter() != 2) return 1;

    // The short-relative BSR saves the sequential PC and returns through RTS.
    words[0] = 0x000BF080; // BSR absolute P:$0020
    words[1] = 0x00000020;
    words[4] = 0x0000000C; // RTS
    words[0x20] = 0x00050BC4; // BSR short-relative -28 -> P:$0004
    words[0x21] = 0x0000000C; // RTS
    xbe::kernel::ApuDspCore short_bsr_core;
    if (!short_bsr_core.bootstrap(table_physical, 1) ||
        short_bsr_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        short_bsr_core.program_counter() != 0x20 ||
        short_bsr_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        short_bsr_core.program_counter() != 4 ||
        short_bsr_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        short_bsr_core.program_counter() != 0x21 ||
        short_bsr_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        short_bsr_core.program_counter() != 2) return 18;

    words[0] = 0x0008F486; // MOVEP #$000001, X:$C6
    words[1] = 0x00000001;
    words[2] = 0x000CC620; // BRSET #0, X:$06, P[+4]
    words[3] = 0x00000004;
    xbe::kernel::ApuDspCore branch_core;
    if (!branch_core.bootstrap(table_physical, 1)) return 1;
    if (branch_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        branch_core.program_counter() != 2) return 1;
    if (branch_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        branch_core.program_counter() != 6) return 1;

    // GP firmware polls the FROZEN status after writing action 3 to X:$D6.
    words[0] = 0x0008F496; // MOVEP #$000003, X:$D6
    words[1] = 0x00000003;
    words[2] = 0x00084E16; // MOVEP X:$D6, R14
    words[3] = 0x0008CE16; // MOVEP R14, X:$D6
    words[4] = 0x000CD603; // BRCLR #3, X:$D6, P[+0]
    words[5] = 0x00000000;
    xbe::kernel::ApuDspCore dma_peripheral_core;
    if (!dma_peripheral_core.bootstrap(table_physical, 1)) return 1;
    if (dma_peripheral_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        (dma_peripheral_core.peripheral_word(false, 0x16) &
         xbe::kernel::ApuDspDma::control_frozen) == 0) return 10;
    if (dma_peripheral_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        dma_peripheral_core.program_counter() != 3 ||
        dma_peripheral_core.register_word(12) != 0x08) return 11;
    if (dma_peripheral_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        dma_peripheral_core.program_counter() != 4) return 12;
    if (dma_peripheral_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        dma_peripheral_core.program_counter() != 6) return 13;

    // MOVEP through the full A alias must read/write its 56-bit accumulator
    // parts, not the unused standalone register slot R14.
    words[0] = 0x0008F486; // MOVEP #$123456, X:$06
    words[1] = 0x00123456;
    words[2] = 0x00084E06; // MOVEP X:$06, A
    words[3] = 0x0008CE07; // MOVEP A, X:$07
    xbe::kernel::ApuDspCore accumulator_movep_core;
    if (!accumulator_movep_core.bootstrap(table_physical, 1) ||
        accumulator_movep_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        accumulator_movep_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        accumulator_movep_core.register_word(8) != 0 ||
        accumulator_movep_core.register_word(12) != 0x123456 ||
        accumulator_movep_core.register_word(10) != 0 ||
        accumulator_movep_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        accumulator_movep_core.peripheral_word(false, 0x07) != 0x123456) return 19;

    // PM2 immediate MOVE #xx,D must update the selected DSP register.
    words[0] = 0x00240100; // MOVE #$01,R4
    words[1] = 0x00240000; // MOVE #$00,R4
    xbe::kernel::ApuDspCore pm2_immediate_core;
    if (!pm2_immediate_core.bootstrap(table_physical, 1) ||
        pm2_immediate_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        pm2_immediate_core.register_word(4) != 1 ||
        pm2_immediate_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        pm2_immediate_core.program_counter() != 2 ||
        pm2_immediate_core.register_word(4) != 0) return 14;

    // MOVEP's absolute EA immediate form writes a 24-bit literal to X/Y:$pp.
    words[0] = 0x0007F432; // MOVEP #$FFFFFF, X:$32
    words[1] = 0x00FFFFFF;
    xbe::kernel::ApuDspCore movep_absolute_core;
    if (!movep_absolute_core.bootstrap(table_physical, 1) ||
        movep_absolute_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        movep_absolute_core.program_counter() != 2 ||
        movep_absolute_core.peripheral_word(false, 0x32) != 0xFFFFFF) return 15;

    // A frame starts by setting the C5 interrupt bit; JCLR polls that bit.
    words[0] = 0x0008F485; // MOVEP #$000002, X:$C5 (write-one-to-clear)
    words[1] = 0x00000002;
    words[2] = 0x000A8581; // JCLR #1, X:$C5, P:$0020
    words[3] = 0x00000020;
    xbe::kernel::ApuDspCore frame_core;
    if (!frame_core.bootstrap(table_physical, 1)) return 16;
    frame_core.start_frame();
    if (frame_core.peripheral_word(false, 5) != 2) return 171;
    if (frame_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        frame_core.peripheral_word(false, 5) != 0) return 172;
    if (frame_core.step() != xbe::kernel::ApuDspCore::StepResult::executed ||
        frame_core.program_counter() != 0x20) return 173;

    xbe::kernel::physical_memory_unregister(scratch);
    xbe::kernel::physical_memory_unregister(table);
    std::free(scratch);
    std::free(table);
    return 0;
}
