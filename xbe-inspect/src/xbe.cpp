#include "xbe.hpp"
#include <fstream>
#include <iostream>
#include <cstring>

namespace xbe {

static std::string utf16le_to_ascii(const char16_t* str, size_t max_len) {
    std::string out;
    for (size_t i = 0; i < max_len; ++i) {
        if (str[i] == 0) break;
        // Basic fallback to ASCII for terminal printing
        if (str[i] < 128) {
            out.push_back(static_cast<char>(str[i]));
        } else {
            out.push_back('?');
        }
    }
    return out;
}

ParseResult parse_file(const std::string& filepath) {
    ParseResult result{};
    result.valid = false;

    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        result.error_message = "Dosya acilamadi: " + filepath;
        return result;
    }

    Header header;
    file.read(reinterpret_cast<char*>(&header), sizeof(Header));
    
    if (file.gcount() < static_cast<std::streamsize>(sizeof(Header))) {
        result.error_message = "Dosya boyutu XBE basligi icin cok kucuk.";
        return result;
    }

    if (header.magic != MAGIC_XBEH) {
        result.error_message = "Gecersiz magic number! (Beklenen: XBEH)";
        return result;
    }

    result.base_address = header.base_address;
    result.entry_point_raw = header.entry_point;
    result.entry_point_retail = header.entry_point ^ ENTRY_POINT_RETAIL_KEY;
    result.entry_point_debug = header.entry_point ^ ENTRY_POINT_DEBUG_KEY;

    result.kernel_thunk_address_raw = 0;
    result.kernel_thunk_address_retail = 0;

    // Header kismi icin basit VA -> Offset (Sadece Base Address bazli)
    auto header_va_to_offset = [&header](uint32_t va) -> uint32_t {
        if (va >= header.base_address && va < header.base_address + header.sizeof_headers) {
            return va - header.base_address;
        }
        return 0; 
    };

    // 1. Read Certificate (Header icindedir)
    uint32_t cert_offset = header_va_to_offset(header.certificate_address);
    if (cert_offset > 0) {
        file.clear();
        file.seekg(cert_offset, std::ios::beg);
        Certificate cert;
        file.read(reinterpret_cast<char*>(&cert), sizeof(Certificate));
        
        if (file.gcount() == sizeof(Certificate)) {
            result.title_name = utf16le_to_ascii(cert.title_name, 40);
        } else {
            result.title_name = "<Sertifika Okunamadi>";
        }
    }

    // 2. Read Section Headers (Header icindedir)
    std::vector<SectionHeader> sec_headers;
    uint32_t sec_hdr_offset = header_va_to_offset(header.section_headers_address);
    if (sec_hdr_offset > 0 && header.number_of_sections > 0 && header.number_of_sections < 256) {
        file.clear();
        file.seekg(sec_hdr_offset, std::ios::beg);
        
        sec_headers.resize(header.number_of_sections);
        file.read(reinterpret_cast<char*>(sec_headers.data()), header.number_of_sections * sizeof(SectionHeader));
    }

    // Ilerlemis VA -> Offset (Hem header hem de Sectionlari kontrol eder)
    auto advanced_va_to_offset = [&header, &sec_headers](uint32_t va) -> uint32_t {
        // Once header icinde mi?
        if (va >= header.base_address && va < header.base_address + header.sizeof_headers) {
            return va - header.base_address;
        }
        // Degilse section'lar icinde mi?
        for (const auto& sec : sec_headers) {
            if (va >= sec.virtual_address && va < sec.virtual_address + sec.virtual_size) {
                return sec.raw_address + (va - sec.virtual_address);
            }
        }
        // Fallback
        if (va >= header.base_address) return va - header.base_address;
        return 0;
    };

    // 3. Section bilgilerini (Isimler vs.) doldur
    for (uint32_t i = 0; i < sec_headers.size(); ++i) {
        SectionInfo info;
        info.virtual_address = sec_headers[i].virtual_address;
        info.virtual_size = sec_headers[i].virtual_size;
        info.raw_address = sec_headers[i].raw_address;
        info.raw_size = sec_headers[i].raw_size;

        uint32_t name_offset = advanced_va_to_offset(sec_headers[i].section_name_address);
        if (name_offset > 0) {
            file.clear();
            file.seekg(name_offset, std::ios::beg);
            
            char ch;
            while (file.get(ch) && ch != '\0') {
                info.name.push_back(ch);
                if (info.name.size() > 255) break; // sanity limit
            }
        }
        if (info.name.empty()) {
            info.name = "<Bilinmiyor>";
        }
        result.sections.push_back(info);
    }

    // 4. Kernel Image Thunk Adresini Bul (Direct file read & Brute-force)
    uint32_t thunk_raw = 0;
    file.clear();
    file.seekg(0x164, std::ios::beg);
    file.read(reinterpret_cast<char*>(&thunk_raw), sizeof(uint32_t));

    uint32_t thunk_va_retail = 0;
    
    auto is_valid_va = [&sec_headers](uint32_t va) -> bool {
        for (const auto& sec : sec_headers) {
            if (va >= sec.virtual_address && va < sec.virtual_address + sec.virtual_size) return true;
        }
        return false;
    };

    auto try_decode = [&is_valid_va, &advanced_va_to_offset, &file](uint32_t raw, uint32_t& out_va) -> bool {
        uint32_t test_va = 0;
        if (is_valid_va(raw)) test_va = raw;
        else if (is_valid_va(raw ^ 0x5B6D40B6)) test_va = raw ^ 0x5B6D40B6;
        else if (is_valid_va(raw ^ 0xEFB1F152)) test_va = raw ^ 0xEFB1F152;
        else return false;
        
        uint32_t offset = advanced_va_to_offset(test_va);
        if (offset == 0) return false;
        
        uint32_t first_thunk = 0;
        auto cur_pos = file.tellg();
        file.clear();
        file.seekg(offset, std::ios::beg);
        file.read(reinterpret_cast<char*>(&first_thunk), 4);
        file.clear();
        file.seekg(cur_pos, std::ios::beg);
        
        // Ordinal numarasi (kernel export'u) genelde 1 - 400 arasidir, ve high-bit (0x80000000) setlidir.
        if ((first_thunk & 0x80000000) != 0 && (first_thunk & 0x7FFFFFFF) < 500) {
            out_va = test_va;
            return true;
        }
        return false;
    };

    if (!try_decode(thunk_raw, thunk_va_retail)) {
        std::cout << "[DEBUG] 0x164 offsetindeki thunk degeri (" << std::hex << thunk_raw << ") gecerli degil. 0x150-0x180 arasi taranacak...\n";
        bool found = false;
        for (uint32_t offset = 0x150; offset <= 0x180; offset += 4) {
            uint32_t test_raw = 0;
            file.clear();
            file.seekg(offset, std::ios::beg);
            file.read(reinterpret_cast<char*>(&test_raw), sizeof(uint32_t));
            
            if (test_raw != 0 && try_decode(test_raw, thunk_va_retail)) {
                std::cout << "[DEBUG] Gecerli Thunk Adresi offset 0x" << std::hex << offset 
                          << " konumunda bulundu! Raw: 0x" << test_raw 
                          << ", Decoded VA: 0x" << thunk_va_retail << std::dec << "\n";
                thunk_raw = test_raw;
                found = true;
                break;
            }
        }
        if (!found) {
            std::cout << "[DEBUG] DIKKAT: Gecerli Thunk Adresi HICBIR YERDE BULUNAMADI!\n";
        }
    } else {
        std::cout << "[DEBUG] Thunk Adresi 0x164'ten dogrulandi. Raw: 0x" << std::hex << thunk_raw 
                  << ", Decoded VA: 0x" << thunk_va_retail << std::dec << "\n";
    }

    result.kernel_thunk_address_raw = thunk_raw;
    result.kernel_thunk_address_retail = thunk_va_retail;

    // 4. Kernel Thunks Oku (Genelde bir section icindedir)
    uint32_t thunk_offset = advanced_va_to_offset(result.kernel_thunk_address_retail);
    std::cout << "[DEBUG] Kernel Thunk Dosya Ofseti (File Offset): 0x" << std::hex << thunk_offset << std::dec << "\n";
    
    if (thunk_offset > 0) {
        file.clear();
        file.seekg(thunk_offset, std::ios::beg);
        uint32_t thunk_val = 0;
        int read_count = 0;
        while (file.read(reinterpret_cast<char*>(&thunk_val), sizeof(uint32_t))) {
            if (thunk_val == 0) {
                std::cout << "[DEBUG] Thunk tablosunda 0 okundu, tablo sonu.\n";
                break;
            }
            uint32_t ordinal = thunk_val & 0x7FFFFFFF;
            result.kernel_imports.push_back(ordinal);
            read_count++;
        }
        if (file.eof() || file.fail()) {
            std::cout << "[DEBUG] Thunk okuma sirasinda EOF veya FAIL alindi! Okunan sayi: " << read_count << "\n";
        }
        file.clear(); // EOF sonrasi fail durumunu temizle
    }

    result.valid = true;
    return result;
}

} // namespace xbe
