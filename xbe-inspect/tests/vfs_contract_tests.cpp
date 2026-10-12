#include "../src/kernel/vfs.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
using xbe::kernel::vfs_init;
using xbe::kernel::vfs_register_symlink;
using xbe::kernel::vfs_set_host_root;
using xbe::kernel::vfs_translate_path;

static void expect_equal(const std::string& actual, const std::string& expected, const char* behavior) {
    if (actual != expected) {
        throw std::runtime_error(std::string(behavior) + ": expected '" + expected + "', got '" + actual + "'");
    }
}

int main() {
    const fs::path root = fs::temp_directory_path() / ("xxcl-vfs-tests-" + std::to_string(getpid()));
    fs::remove_all(root);
    fs::create_directories(root);

    try {
        vfs_set_host_root(root.string());
        vfs_init();

        if (!fs::is_directory(root / "D") || !fs::is_directory(root / "E") || !fs::is_directory(root / "C")) {
            throw std::runtime_error("VFS initialization did not create all drive roots");
        }

        expect_equal(
            vfs_translate_path("\\Device\\Harddisk0\\partition1\\UDATA\\game\\save.dat"),
            (root / "E" / "UDATA" / "game" / "save.dat").string(),
            "hard disk device path mapping");

        expect_equal(
            vfs_translate_path("d:\\DATA\\cars.afs"),
            (root / "D" / "DATA" / "cars.afs").string(),
            "drive-letter path mapping");

        vfs_register_symlink("\\??\\D:", "\\Device\\CdRom0");
        expect_equal(
            vfs_translate_path("\\??\\D:\\DATA\\cars.afs"),
            (root / "D" / "DATA" / "cars.afs").string(),
            "Xbox symbolic-link path mapping");
        if (!xbe::kernel::vfs_is_cdrom_path("\\??\\D:\\DATA\\cars.afs") ||
            xbe::kernel::vfs_is_cdrom_path("E:\\UDATA\\save.dat")) {
            throw std::runtime_error("VFS drive classification did not identify optical media");
        }

        expect_equal(vfs_translate_path("\\Device\\Unknown\\file"), "", "unknown device rejection");
        expect_equal(vfs_translate_path("D:\\..\\..\\outside"), "", "host-root traversal rejection");
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        fs::remove_all(root);
        return 1;
    }

    fs::remove_all(root);
    return 0;
}
