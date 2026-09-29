#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace xbe {

const uint32_t MAGIC_XBEH = 0x48454258; // "XBEH"
const uint32_t ENTRY_POINT_RETAIL_KEY = 0xA8FC57AB;
const uint32_t ENTRY_POINT_DEBUG_KEY = 0x94859D4B;

#pragma pack(push, 1)

struct Header {
    uint32_t magic;
    uint8_t  digital_signature[256];
    uint32_t base_address;
    uint32_t sizeof_headers;
    uint32_t sizeof_image;
    uint32_t sizeof_image_header;
    uint32_t timedatestamp;
    uint32_t certificate_address;
    uint32_t number_of_sections;
    uint32_t section_headers_address;
    uint32_t init_flags;
    uint32_t entry_point;
    uint32_t tls_directory_address;
    uint32_t pe_stack_commit;
    uint32_t pe_stack_reserve;
    uint32_t pe_heap_commit;
    uint32_t pe_heap_reserve;
    uint32_t pe_image_base;
    uint32_t pe_image_size;
    uint32_t pe_checksum;
    uint32_t pe_timedatestamp;
    uint32_t debug_pathname_address; // 0x150
    uint32_t debug_filename_address; // 0x154
    uint32_t debug_unicode_filename_address; // 0x158
    uint32_t cg_logo_address; // 0x15C
    uint32_t cg_logo_size; // 0x160
    uint32_t kernel_image_thunk_address; // 0x164
    uint32_t non_kernel_import_dir_address; // 0x168
    uint32_t number_of_library_versions; // 0x16C
    uint32_t library_versions_address; // 0x170
    uint32_t kernel_library_version_address; // 0x174
    uint32_t xapi_library_version_address; // 0x178
    uint32_t logo_bitmap_address; // 0x17C
    uint32_t logo_bitmap_size; // 0x180
};

struct Certificate {
    uint32_t size;
    uint32_t timedatestamp;
    uint32_t title_id;
    char16_t title_name[40];
    uint32_t alternate_title_ids[16];
    uint32_t allowed_media;
    uint32_t game_region;
    uint32_t game_ratings;
    uint32_t disk_number;
    uint32_t version;
    uint8_t  lan_key[16];
    uint8_t  signature_key[16];
    uint8_t  title_alternate_signature_keys[16][16];
};

struct SectionHeader {
    uint32_t flags;
    uint32_t virtual_address;
    uint32_t virtual_size;
    uint32_t raw_address;
    uint32_t raw_size;
    uint32_t section_name_address;
    uint32_t section_name_reference_count;
    uint32_t head_shared_page_reference_count_address;
    uint32_t tail_shared_page_reference_count_address;
    uint8_t  section_digest[20];
};

#pragma pack(pop)

struct SectionInfo {
    std::string name;
    uint32_t virtual_address;
    uint32_t virtual_size;
    uint32_t raw_address;
    uint32_t raw_size;
};

struct ParseResult {
    bool valid;
    std::string error_message;

    uint32_t base_address;
    uint32_t entry_point_raw;
    uint32_t entry_point_retail;
    uint32_t entry_point_debug;
    
    uint32_t kernel_thunk_address_raw;
    uint32_t kernel_thunk_address_retail;
    std::vector<uint32_t> kernel_imports;
    
    std::string title_name;
    std::vector<SectionInfo> sections;
};

ParseResult parse_file(const std::string& filepath);

} // namespace xbe
