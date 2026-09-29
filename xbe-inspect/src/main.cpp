#include "loader.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include <thread>

// ANSI Color Codes
const std::string RESET = "\033[0m";
const std::string BOLD = "\033[1m";
const std::string RED = "\033[31m";
const std::string GREEN = "\033[32m";
const std::string YELLOW = "\033[33m";
const std::string BLUE = "\033[34m";
const std::string CYAN = "\033[36m";

void print_hex_row(const std::string& label, uint32_t value, const std::string& color = RESET) {
    std::cout << BOLD << std::left << std::setfill(' ') << std::setw(25) << label << RESET 
              << ": " << color << "0x" << std::right << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << value << RESET << "\n";
}

void print_str_row(const std::string& label, const std::string& value, const std::string& color = RESET) {
    std::cout << BOLD << std::left << std::setfill(' ') << std::setw(25) << label << RESET 
              << ": " << color << value << RESET << "\n";
}

int main(int argc, char* argv[]) {
    bool run_mode = false;
    std::string filepath;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--run") {
            run_mode = true;
        } else {
            filepath = arg;
        }
    }

    if (filepath.empty()) {
        std::cerr << RED << "Hata: XBE dosyasi belirtilmedi.\n" << RESET
                  << "Kullanim: " << argv[0] << " <dosya.xbe> [--run]\n";
        return 1;
    }
    auto res = xbe::parse_file(filepath);

    if (!res.valid) {
        std::cerr << RED << "[XBE PARSE HATASI] " << res.error_message << RESET << "\n";
        return 1;
    }

    std::cout << "\n" << CYAN << "=== XBE INsPECTOR ===" << RESET << "\n\n";

    // Genel Bilgiler
    std::cout << YELLOW << "[Genel Bilgiler]" << RESET << "\n";
    print_str_row("Dosya Adi", filepath);
    print_str_row("Title Name (Oyun Adi)", res.title_name.empty() ? "<Bos>" : res.title_name, GREEN);
    print_hex_row("Base Address", res.base_address, CYAN);
    std::cout << "\n";

    // Entry Point Bilgileri
    std::cout << YELLOW << "[Entry Point]" << RESET << "\n";
    print_hex_row("Entry Point (Raw)", res.entry_point_raw);
    print_hex_row("Entry Point (Retail)", res.entry_point_retail, GREEN);
    print_hex_row("Entry Point (Debug)", res.entry_point_debug, BLUE);
    std::cout << "\n";

    // Kernel Bilgileri
    std::cout << YELLOW << "[Kernel Image Thunk]" << RESET << "\n";
    print_hex_row("Thunk Addr (Raw)", res.kernel_thunk_address_raw);
    print_hex_row("Thunk Addr (Retail)", res.kernel_thunk_address_retail, GREEN);
    std::cout << "\n";

    if (!res.kernel_imports.empty()) {
        std::cout << YELLOW << "[Import Edilen Kernel Fonksiyonlari (" << std::dec << res.kernel_imports.size() << " Ordinal)]" << RESET << "\n";
        int count = 0;
        for (uint32_t ord : res.kernel_imports) {
            std::cout << "Ordinal: " << std::dec << std::setw(3) << std::setfill(' ') << ord << "   ";
            if (++count % 4 == 0) std::cout << "\n";
        }
        if (count % 4 != 0) std::cout << "\n";
        std::cout << "\n";
    }

    // Section Tablosu
    std::cout << YELLOW << "[Section Tablosu (" << std::dec << res.sections.size() << " Bolum)]" << RESET << "\n";
    
    // Tablo Basligi
    std::cout << BOLD << std::left << std::setfill(' ')
              << std::setw(15) << "Isim"
              << std::setw(15) << "V.Address"
              << std::setw(15) << "V.Size"
              << std::setw(15) << "R.Address"
              << std::setw(15) << "R.Size"
              << RESET << "\n";
    
    std::cout << std::string(75, '-') << "\n";

    for (const auto& sec : res.sections) {
        std::cout << std::left << std::setfill(' ') << std::setw(15) << sec.name;

        std::cout << "0x" << std::right << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << sec.virtual_address << "   ";
        std::cout << "0x" << std::right << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << sec.virtual_size << "   ";
        std::cout << "0x" << std::right << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << sec.raw_address << "   ";
        std::cout << "0x" << std::right << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << sec.raw_size << "\n";
    }
    
    std::cout << std::endl;

    if (run_mode) {
        std::cout << YELLOW << "\n[Baslatiliyor...]" << RESET << "\n";
        xbe::load_and_run(res, filepath);
    }
    return 0;
}
