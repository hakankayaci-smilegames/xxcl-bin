#include "handles.hpp"

#include <cerrno>
#include <cstdint>
#include <map>
#include <mutex>
#include <unistd.h>

namespace xbe::kernel {

namespace {

struct HandleRecord {
    bool thread;
    FileHandleRecord file;
    pthread_t pthread;
};

std::map<void*, HandleRecord> g_handles;
std::mutex g_handles_mutex;
uintptr_t g_next_handle = 0x10000;

void* allocate_handle(const HandleRecord& record) {
    std::lock_guard<std::mutex> lock(g_handles_mutex);
    void* handle = reinterpret_cast<void*>(g_next_handle++);
    g_handles.emplace(handle, record);
    return handle;
}

} // namespace

void* register_file_handle(int fd, bool cdrom) {
    return allocate_handle({false, {fd, cdrom}, {}});
}

void* register_thread_handle(pthread_t thread) {
    return allocate_handle({true, {-1, false}, thread});
}

bool lookup_file_handle(void* handle, FileHandleRecord& record) {
    std::lock_guard<std::mutex> lock(g_handles_mutex);
    const auto it = g_handles.find(handle);
    if (it == g_handles.end() || it->second.thread) return false;
    record = it->second.file;
    return true;
}

int close_kernel_handle(void* handle) {
    HandleRecord record{};
    {
        std::lock_guard<std::mutex> lock(g_handles_mutex);
        const auto it = g_handles.find(handle);
        if (it == g_handles.end()) return EBADF;
        record = it->second;
        g_handles.erase(it);
    }
    if (record.thread) return 0;
    return close(record.file.fd) == 0 ? 0 : errno;
}

} // namespace xbe::kernel
