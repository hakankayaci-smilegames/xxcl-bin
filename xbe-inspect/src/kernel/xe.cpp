#include "xe.hpp"
#include <cstdio>
#include "../kernel.hpp"

namespace xbe {
namespace kernel {

extern "C" uint32_t __attribute__((__stdcall__)) xbox_XeLoadSection(xbe::kernel::XBE_SECTION_HEADER* Section) {
    xbe::record_kernel_call(327);
    if (!Section) return 0;

    const char* sec_name = (const char*)Section->SectionNameAddress;
    printf("[KERNEL] XeLoadSection -> Bolum: '%s' (VA: 0x%08X, Boyut: 0x%X, Ref: %u)\n",
           sec_name ? sec_name : "UNKNOWN",
           Section->VirtualAddress,
           Section->VirtualSize,
           Section->SectionReferenceCount);

    Section->SectionReferenceCount++;

    return 1;
}

extern "C" uint32_t __attribute__((__stdcall__)) xbox_XeUnloadSection(xbe::kernel::XBE_SECTION_HEADER* Section) {
    xbe::record_kernel_call(328);
    if (!Section) return 0;

    const char* sec_name = (const char*)Section->SectionNameAddress;
    printf("[KERNEL] XeUnloadSection -> Bolum: '%s' (Ref: %u)\n",
           sec_name ? sec_name : "UNKNOWN", Section->SectionReferenceCount);

    if (Section->SectionReferenceCount > 0) {
        Section->SectionReferenceCount--;
    }
    
    return 1;
}

} // namespace kernel
} // namespace xbe
