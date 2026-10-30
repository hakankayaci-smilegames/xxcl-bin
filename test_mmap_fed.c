#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

int main() {
    void* mmio = mmap((void*)0xFED00000, 0x00010000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (mmio == MAP_FAILED) {
        printf("mmap failed: %s\n", strerror(errno));
    } else {
        printf("mmap success at %p\n", mmio);
    }
    return 0;
}
