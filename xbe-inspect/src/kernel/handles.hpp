#pragma once

#include <pthread.h>

namespace xbe::kernel {

struct FileHandleRecord {
    int fd;
    bool cdrom;
};

void* register_file_handle(int fd, bool cdrom);
void* register_thread_handle(pthread_t thread);
bool lookup_file_handle(void* handle, FileHandleRecord& record);
int close_kernel_handle(void* handle);

} // namespace xbe::kernel
