#include "../src/kernel/io.hpp"
#include "../src/kernel/vfs.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace xbe::kernel;

namespace xbe {
void record_kernel_call(uint32_t) {}
}

extern "C" int32_t __attribute__((__stdcall__)) xbox_KeSetEvent(void*, int32_t, uint8_t) {
    return 0;
}

static void expect_status(NTSTATUS actual, NTSTATUS expected, const char* behavior) {
    if (actual != expected) {
        throw std::runtime_error(std::string(behavior) + ": expected status 0x" +
            std::to_string(static_cast<uint32_t>(expected)) + ", got 0x" +
            std::to_string(static_cast<uint32_t>(actual)));
    }
}

static OBJECT_ATTRIBUTES object_attributes(STRING& name) {
    OBJECT_ATTRIBUTES attributes{};
    attributes.ObjectName = &name;
    return attributes;
}

int main() {
    const fs::path root = fs::temp_directory_path() / ("xxcl-io-tests-" + std::to_string(getpid()));
    fs::remove_all(root);
    fs::create_directories(root / "D");
    xbe::kernel::vfs_set_host_root(root.string());
    xbe::kernel::vfs_init();

    try {
        std::string missing_path = "D:\\missing.dat";
        STRING missing_name{static_cast<uint16_t>(missing_path.size()), static_cast<uint16_t>(missing_path.size()), missing_path.data()};
        OBJECT_ATTRIBUTES missing_attributes = object_attributes(missing_name);
        IO_STATUS_BLOCK iosb{};
        void* handle = nullptr;
        const NTSTATUS not_found = static_cast<NTSTATUS>(0xC0000034);
        expect_status(xbe::kernel::xbox_NtOpenFile(&handle, 0x80000000, &missing_attributes, &iosb, 0, 0),
                      not_found, "NtOpenFile reports missing files");
        expect_status(iosb.Status, not_found, "NtOpenFile records the failure in IO_STATUS_BLOCK");
        if (handle != nullptr) throw std::runtime_error("NtOpenFile returned a handle for a missing file");

        const fs::path file_path = root / "E" / "item.bin";
        {
            std::ofstream file(file_path, std::ios::binary);
            file << "abcdef";
        }

        std::string file_name_text = "E:\\item.bin";
        STRING file_name{static_cast<uint16_t>(file_name_text.size()), static_cast<uint16_t>(file_name_text.size()), file_name_text.data()};
        OBJECT_ATTRIBUTES file_attributes = object_attributes(file_name);
        handle = nullptr;
        expect_status(xbe::kernel::xbox_NtCreateFile(&handle, 0x40000000, &file_attributes, &iosb, nullptr,
                      0, 0, 2, 0, nullptr, 0), static_cast<NTSTATUS>(0xC0000035),
                      "FILE_CREATE rejects an existing file");
        if (handle != nullptr) throw std::runtime_error("FILE_CREATE returned a handle for an existing file");

        expect_status(xbe::kernel::xbox_NtCreateFile(&handle, 0xC0000000, &file_attributes, &iosb, nullptr,
                      0, 0, 4, 0, nullptr, 0), 0, "FILE_OVERWRITE opens existing file");
        if (fs::file_size(file_path) != 0) throw std::runtime_error("FILE_OVERWRITE did not truncate the existing file");

        const char replacement[] = "xy";
        int64_t offset = 2;
        expect_status(xbe::kernel::xbox_NtWriteFile(handle, nullptr, nullptr, nullptr, &iosb,
                      const_cast<char*>(replacement), 2, &offset, nullptr), 0, "NtWriteFile writes at explicit offsets");
        if (iosb.Information != 2) throw std::runtime_error("NtWriteFile returned the wrong byte count");

        int64_t read_offset = 2;
        char read_buffer[3]{};
        expect_status(xbe::kernel::xbox_NtReadFile(handle, nullptr, nullptr, nullptr, &iosb,
                      read_buffer, 2, &read_offset, nullptr), 0, "NtReadFile reads at explicit offsets");
        if (std::string(read_buffer, 2) != "xy" || iosb.Information != 2) {
            throw std::runtime_error("NtReadFile returned the wrong bytes or byte count");
        }

        char sequential_read[4]{};
        expect_status(xbe::kernel::xbox_NtReadFile(handle, nullptr, nullptr, nullptr, &iosb,
                      sequential_read, sizeof(sequential_read), nullptr, nullptr), 0,
                      "NtReadFile advances the current file position");
        if (iosb.Information != 4 || sequential_read[0] != 0 || sequential_read[1] != 0 ||
            sequential_read[2] != 'x' || sequential_read[3] != 'y') {
            throw std::runtime_error("NtReadFile current-position result was incorrect");
        }
        const char sequential_write[] = "z";
        expect_status(xbe::kernel::xbox_NtWriteFile(handle, nullptr, nullptr, nullptr, &iosb,
                      const_cast<char*>(sequential_write), 1, nullptr, nullptr), 0,
                      "NtWriteFile advances the current file position");

        struct StandardInformation {
            uint64_t AllocationSize;
            uint64_t EndOfFile;
            uint32_t NumberOfLinks;
            uint8_t DeletePending;
            uint8_t Directory;
            uint8_t Reserved[2];
        } standard_info{};
        expect_status(xbe::kernel::xbox_NtQueryInformationFile(handle, &iosb, &standard_info,
                      sizeof(standard_info), 5), 0, "FileStandardInformation is supported");
        if (standard_info.EndOfFile != 5 || standard_info.Directory != 0 ||
            iosb.Information != sizeof(standard_info)) {
            throw std::runtime_error("FileStandardInformation returned incorrect metadata");
        }

        void* write_handle = nullptr;
        const fs::path disc_file_path = root / "D" / "item.bin";
        { std::ofstream disc_file(disc_file_path, std::ios::binary); disc_file << "disc"; }
        std::string disc_file_name_text = "D:\\item.bin";
        STRING disc_file_name{static_cast<uint16_t>(disc_file_name_text.size()),
                              static_cast<uint16_t>(disc_file_name_text.size()), disc_file_name_text.data()};
        OBJECT_ATTRIBUTES disc_file_attributes = object_attributes(disc_file_name);
        expect_status(xbe::kernel::xbox_NtCreateFile(&write_handle, 0x40000000, &disc_file_attributes, &iosb,
                      nullptr, 0, 0, 1, 0, nullptr, 0), static_cast<NTSTATUS>(0xC00000A2),
                      "CD-ROM paths reject write access");
        if (write_handle != nullptr || fs::file_size(disc_file_path) != 4) {
            throw std::runtime_error("CD-ROM write request changed the disc file");
        }
        void* read_only_disc_handle = nullptr;
        expect_status(xbe::kernel::xbox_NtOpenFile(&read_only_disc_handle, 0x80000000,
                      &disc_file_attributes, &iosb, 0, 0), 0, "CD-ROM opens for reading");
        const char denied_write[] = "x";
        expect_status(xbe::kernel::xbox_NtWriteFile(read_only_disc_handle, nullptr, nullptr, nullptr,
                      &iosb, const_cast<char*>(denied_write), 1, nullptr, nullptr),
                      static_cast<NTSTATUS>(0xC00000A2), "NtWriteFile rejects CD-ROM handles");
        xbe::kernel::xbox_NtClose(read_only_disc_handle);

        struct SizeInformation {
            uint64_t TotalAllocationUnits;
            uint64_t AvailableAllocationUnits;
            uint32_t SectorsPerAllocationUnit;
            uint32_t BytesPerSector;
        } size_info{};
        expect_status(xbe::kernel::xbox_NtQueryVolumeInformationFile(handle, &iosb, &size_info,
                      sizeof(size_info), 3), 0, "FileFsSizeInformation is supported");
        if (size_info.SectorsPerAllocationUnit == 0 || size_info.BytesPerSector != 512 ||
            size_info.TotalAllocationUnits == 0 ||
            iosb.Information != sizeof(size_info)) {
            throw std::runtime_error("FileFsSizeInformation returned invalid host volume geometry");
        }

        expect_status(xbe::kernel::xbox_NtClose(handle), 0, "NtClose closes tracked handles");
        expect_status(xbe::kernel::xbox_NtReadFile(handle, nullptr, nullptr, nullptr, &iosb,
                      read_buffer, 2, &read_offset, nullptr), static_cast<NTSTATUS>(0xC0000008),
                      "closed handles are rejected");
        expect_status(xbe::kernel::xbox_NtClose(handle), static_cast<NTSTATUS>(0xC0000008),
                      "NtClose rejects already closed handles");

        std::string drive_path = "D:\\";
        STRING drive_name{static_cast<uint16_t>(drive_path.size()), static_cast<uint16_t>(drive_path.size()), drive_path.data()};
        OBJECT_ATTRIBUTES drive_attributes = object_attributes(drive_name);
        void* cdrom_handle = nullptr;
        expect_status(xbe::kernel::xbox_NtOpenFile(&cdrom_handle, 0x80000000, &drive_attributes,
                      &iosb, 0, 1), 0, "NtOpenFile opens the CD-ROM root as a directory");
        struct DeviceInformation { uint32_t DeviceType; uint32_t Characteristics; } device_info{};
        expect_status(xbe::kernel::xbox_NtQueryVolumeInformationFile(cdrom_handle, &iosb, &device_info,
                      sizeof(device_info), 4), 0, "FileFsDeviceInformation identifies the CD-ROM");
        if (device_info.DeviceType != 2 || (device_info.Characteristics & 0x2) == 0) {
            throw std::runtime_error("FileFsDeviceInformation did not identify read-only optical media");
        }
        uint8_t request[32]{};
        uint8_t response[28]{};
        const uint32_t transfer_length = sizeof(response);
        const uint32_t response_address = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(response));
        std::memcpy(request + 12, &transfer_length, sizeof(transfer_length));
        std::memcpy(request + 20, &response_address, sizeof(response_address));
        request[28] = 0x5A;
        request[30] = 0x3E;
        expect_status(xbe::kernel::xbox_NtDeviceIoControlFile(cdrom_handle, nullptr, nullptr, nullptr,
                      &iosb, 0x0004D014, request, sizeof(request), nullptr, 0), 0,
                      "CD-ROM backend handles Xbox media MODE SENSE requests");
        if (request[2] != 0 || response[1] != 26 || response[8] != 0x3E ||
            response[10] != 1 || response[12] != 0x0D || iosb.Information != sizeof(response)) {
            throw std::runtime_error("CD-ROM backend returned an invalid media response");
        }
        expect_status(xbe::kernel::xbox_NtDeviceIoControlFile(cdrom_handle, nullptr, nullptr, nullptr,
                      &iosb, 0xDEADBEEF, request, sizeof(request), nullptr, 0),
                      static_cast<NTSTATUS>(0xC00000BB), "unsupported CD-ROM controls are rejected");
        xbe::kernel::xbox_NtClose(cdrom_handle);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        fs::remove_all(root);
        return 1;
    }

    fs::remove_all(root);
    return 0;
}
