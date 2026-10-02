#include "nv2a.hpp"
#include <sys/mman.h>
#include <cerrno>
#include <cstdio>
#include <cstring>

namespace xbe {
namespace kernel {

void nv2a_init_mmio() {
    // 0xFD000000 - 16MB mapped memory
    void* mmio = mmap((void*)0xFD000000, 0x01000000, PROT_READ | PROT_WRITE, 
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (mmio == MAP_FAILED) {
        printf("[NV2A] MMIO 0xFD000000 mmap BASARISIZ! (Hata: %s)\n", strerror(errno));
    } else {
        printf("[NV2A] NV2A MMIO sahte donanim alani (0xFD000000 - 16MB) basariyla eslendi.\n");
    }
}

} // namespace kernel
} // namespace xbe
