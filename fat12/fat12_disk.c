#include "fat12_disk.h"
#include <furi.h>
#include <string.h>
#include <stdio.h>

#define TAG "FAT12Disk"

static void fat12_set_cluster(uint8_t* fat, uint16_t cluster, uint16_t val) {
    size_t byte_offset = cluster + (cluster / 2);
    if(cluster & 1) {
        fat[byte_offset] = (fat[byte_offset] & 0x0F) | ((val & 0x0F) << 4);
        fat[byte_offset + 1] = (uint8_t)((val >> 4) & 0xFF);
    } else {
        fat[byte_offset] = (uint8_t)(val & 0xFF);
        fat[byte_offset + 1] = (fat[byte_offset + 1] & 0xF0) | ((val >> 8) & 0x0F);
    }
}

static uint16_t fat12_get_cluster(const uint8_t* fat, uint16_t cluster) {
    size_t byte_offset = cluster + (cluster / 2);
    uint16_t raw = (uint16_t)fat[byte_offset] | ((uint16_t)fat[byte_offset + 1] << 8);
    if(cluster & 1) {
        return (raw >> 4) & 0x0FFF;
    } else {
        return raw & 0x0FFF;
    }
}

bool fat12_disk_create(
    const char* img_path,
    const PasswordVault* vault,
    Storage* storage) {
    File* file = storage_file_alloc(storage);
    bool success = false;
    uint8_t* sec = malloc(FAT12_SECTOR_SIZE);
    if(!sec) {
        storage_file_free(file);
        return false;
    }

    do {
        if(!storage_file_open(file, img_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            FURI_LOG_E(TAG, "Cannot create image file: %s", img_path);
            break;
        }

        // Expand file to full disk size (64 KB)
        uint32_t total_bytes = FAT12_TOTAL_SECTORS * FAT12_SECTOR_SIZE;
        if(!storage_file_seek(file, total_bytes - 1, true)) break;
        uint8_t zero = 0;
        if(storage_file_write(file, &zero, 1) != 1) break;
        if(!storage_file_seek(file, 0, true)) break;

        // Build Boot Sector (Sector 0)
        memset(sec, 0, FAT12_SECTOR_SIZE);

        sec[0] = 0xEB; sec[1] = 0x3C; sec[2] = 0x90;
        memcpy(&sec[3], "MSWIN4.1", 8);

        sec[11] = (uint8_t)(FAT12_SECTOR_SIZE & 0xFF);
        sec[12] = (uint8_t)(FAT12_SECTOR_SIZE >> 8);
        sec[13] = 1; // sectors per cluster
        sec[14] = 1; sec[15] = 0; // reserved sectors = 1
        sec[16] = 2; // number of FATs
        sec[17] = (uint8_t)(FAT12_ROOT_ENTRIES & 0xFF);
        sec[18] = (uint8_t)(FAT12_ROOT_ENTRIES >> 8);
        sec[19] = (uint8_t)(FAT12_TOTAL_SECTORS & 0xFF);
        sec[20] = (uint8_t)(FAT12_TOTAL_SECTORS >> 8);
        sec[21] = 0xF8; // Media descriptor (fixed disk)
        sec[22] = FAT12_SECTORS_PER_FAT; sec[23] = 0;
        sec[24] = 18; sec[25] = 0; // sectors per track
        sec[26] = 2; sec[27] = 0;  // heads
        sec[36] = 0x80; // drive number
        sec[38] = 0x29; // boot signature
        sec[39] = 0x78; sec[40] = 0x56; sec[41] = 0x34; sec[42] = 0x12; // vol ID
        memcpy(&sec[43], "FLIPPER_PWD", 11);
        memcpy(&sec[54], "FAT12   ", 8);
        sec[510] = 0x55; sec[511] = 0xAA;

        if(storage_file_write(file, sec, FAT12_SECTOR_SIZE) != FAT12_SECTOR_SIZE) break;

        // Generate passwords.txt content
        size_t content_cap = 4096;
        char* content = malloc(content_cap);
        if(!content) break;
        content[0] = '\0';
        size_t content_len = 0;

        if(vault && vault->entry_count > 0) {
            for(size_t i = 0; i < vault->entry_count; i++) {
                char line[MAX_PASSWORD_LEN + MAX_DESCRIPTION_LEN + 16];
                int line_len = snprintf(
                    line,
                    sizeof(line),
                    "%s:@:%s\r\n",
                    vault->entries[i].password,
                    vault->entries[i].description);
                if(content_len + line_len < content_cap - 1) {
                    memcpy(content + content_len, line, line_len);
                    content_len += line_len;
                    content[content_len] = '\0';
                }
            }
        } else {
            const char* header =
                "# Flipper Password Manager\r\n"
                "# Format: password:@:description\r\n"
                "example123:@:Example Account\r\n";
            content_len = strlen(header);
            memcpy(content, header, content_len + 1);
        }

        uint16_t clusters_needed = (content_len + FAT12_SECTOR_SIZE - 1) / FAT12_SECTOR_SIZE;
        if(clusters_needed == 0) clusters_needed = 1;

        // Build FAT table (1 sector for 64 KB disk)
        uint8_t* fat = malloc(FAT12_SECTORS_PER_FAT * FAT12_SECTOR_SIZE);
        if(!fat) {
            free(content);
            break;
        }
        memset(fat, 0, FAT12_SECTORS_PER_FAT * FAT12_SECTOR_SIZE);
        fat[0] = 0xF8;
        fat[1] = 0xFF;
        fat[2] = 0xFF;

        for(uint16_t c = 0; c < clusters_needed; c++) {
            uint16_t cluster_idx = 2 + c;
            uint16_t next_cluster = (c + 1 < clusters_needed) ? (cluster_idx + 1) : 0x0FFF;
            fat12_set_cluster(fat, cluster_idx, next_cluster);
        }

        // Write FAT1 & FAT2
        storage_file_write(file, fat, FAT12_SECTOR_SIZE);
        storage_file_write(file, fat, FAT12_SECTOR_SIZE);
        free(fat);

        // Build Root Directory (2 sectors = 32 entries)
        uint8_t* root_dir = malloc(FAT12_ROOT_SECTORS * FAT12_SECTOR_SIZE);
        if(!root_dir) {
            free(content);
            break;
        }
        memset(root_dir, 0, FAT12_ROOT_SECTORS * FAT12_SECTOR_SIZE);

        // Entry 0: Volume label
        memcpy(&root_dir[0], "FLIPPER_PWD", 11);
        root_dir[11] = 0x08;

        // Entry 1: PASSWORDS.TXT
        uint8_t* file_ent = &root_dir[32];
        memcpy(&file_ent[0], "PASSWO~1TXT", 11); // 8.3 name
        file_ent[11] = 0x20;
        file_ent[26] = 2; file_ent[27] = 0; // cluster 2
        file_ent[28] = (uint8_t)(content_len & 0xFF);
        file_ent[29] = (uint8_t)((content_len >> 8) & 0xFF);
        file_ent[30] = (uint8_t)((content_len >> 16) & 0xFF);
        file_ent[31] = (uint8_t)((content_len >> 24) & 0xFF);

        // Entry 2: Second alias with PASSWORD.TXT just in case
        uint8_t* file_ent2 = &root_dir[64];
        memcpy(&file_ent2[0], "PASSWORDTXT", 11);
        file_ent2[11] = 0x20;
        file_ent2[26] = 2; file_ent2[27] = 0;
        file_ent2[28] = file_ent[28];
        file_ent2[29] = file_ent[29];
        file_ent2[30] = file_ent[30];
        file_ent2[31] = file_ent[31];

        storage_file_write(file, root_dir, FAT12_ROOT_SECTORS * FAT12_SECTOR_SIZE);
        free(root_dir);

        // Write file data clusters
        size_t written_data = 0;
        while(written_data < (size_t)(clusters_needed * FAT12_SECTOR_SIZE)) {
            memset(sec, 0, FAT12_SECTOR_SIZE);
            if(written_data < content_len) {
                size_t to_copy = content_len - written_data;
                if(to_copy > FAT12_SECTOR_SIZE) to_copy = FAT12_SECTOR_SIZE;
                memcpy(sec, content + written_data, to_copy);
            }
            storage_file_write(file, sec, FAT12_SECTOR_SIZE);
            written_data += FAT12_SECTOR_SIZE;
        }

        free(content);
        success = true;
    } while(false);

    free(sec);
    storage_file_close(file);
    storage_file_free(file);
    return success;
}

static void parse_line(const char* line, size_t len, PasswordVault* vault) {
    if(len == 0 || vault->entry_count >= MAX_VAULT_ENTRIES) return;
    if(line[0] == '#' || line[0] == ';') return;

    const char* found = NULL;
    for(size_t i = 0; i + 2 < len; i++) {
        if(line[i] == ':' && line[i + 1] == '@' && line[i + 2] == ':') {
            found = &line[i];
            break;
        }
    }
    if(!found) return;

    size_t pass_len = found - line;
    const char* desc_start = found + 3;
    size_t desc_len = len - (desc_start - line);

    while(desc_len > 0 && (desc_start[desc_len - 1] == '\r' || desc_start[desc_len - 1] == '\n')) {
        desc_len--;
    }

    if(pass_len == 0 && desc_len == 0) return;

    PasswordEntry* ent = &vault->entries[vault->entry_count];
    memset(ent, 0, sizeof(PasswordEntry));

    if(pass_len >= MAX_PASSWORD_LEN) pass_len = MAX_PASSWORD_LEN - 1;
    memcpy(ent->password, line, pass_len);
    ent->password[pass_len] = '\0';

    if(desc_len >= MAX_DESCRIPTION_LEN) desc_len = MAX_DESCRIPTION_LEN - 1;
    memcpy(ent->description, desc_start, desc_len);
    ent->description[desc_len] = '\0';

    vault->entry_count++;
}

bool fat12_disk_read_vault(
    const char* img_path,
    PasswordVault* vault,
    Storage* storage) {
    File* file = storage_file_alloc(storage);
    bool success = false;

    do {
        if(!storage_file_open(file, img_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURI_LOG_E(TAG, "Cannot open disk image: %s", img_path);
            break;
        }

        uint8_t boot_sec[FAT12_SECTOR_SIZE];
        if(storage_file_read(file, boot_sec, sizeof(boot_sec)) != sizeof(boot_sec)) break;

        uint16_t reserved_secs = boot_sec[14] | (boot_sec[15] << 8);
        uint8_t num_fats = boot_sec[16];
        uint16_t secs_per_fat = boot_sec[22] | (boot_sec[23] << 8);
        uint16_t root_entries = boot_sec[17] | (boot_sec[18] << 8);

        uint32_t fat_start_sec = reserved_secs;
        uint32_t root_start_sec = fat_start_sec + num_fats * secs_per_fat;
        uint32_t root_size_bytes = root_entries * 32;
        uint32_t data_start_sec = root_start_sec + (root_size_bytes + FAT12_SECTOR_SIZE - 1) / FAT12_SECTOR_SIZE;

        size_t fat_size = secs_per_fat * FAT12_SECTOR_SIZE;
        uint8_t* fat = malloc(fat_size);
        if(!fat) break;

        if(!storage_file_seek(file, fat_start_sec * FAT12_SECTOR_SIZE, true) ||
           storage_file_read(file, fat, fat_size) != fat_size) {
            free(fat);
            break;
        }

        uint8_t* root_dir = malloc(root_size_bytes);
        if(!root_dir) {
            free(fat);
            break;
        }

        if(!storage_file_seek(file, root_start_sec * FAT12_SECTOR_SIZE, true) ||
           storage_file_read(file, root_dir, root_size_bytes) != root_size_bytes) {
            free(fat);
            free(root_dir);
            break;
        }

        uint16_t first_cluster = 0;
        uint32_t file_size = 0;
        bool found_file = false;

        for(size_t i = 0; i < root_entries; i++) {
            const uint8_t* ent = &root_dir[i * 32];
            if(ent[0] == 0x00) break;
            if(ent[0] == 0xE5) continue;
            if(ent[11] & 0x08) continue;
            if((ent[11] & 0x0F) == 0x0F) continue; // Skip LFN

            // Match PASSWO~1TXT or PASSWORDTXT or anything starting with PASSW and ending with TXT
            if(memcmp(ent, "PASSWO~1TXT", 11) == 0 ||
               memcmp(ent, "PASSWORDTXT", 11) == 0 ||
               (memcmp(ent, "PASSW", 5) == 0 && memcmp(ent + 8, "TXT", 3) == 0)) {
                first_cluster = ent[26] | (ent[27] << 8);
                file_size = ent[28] | (ent[29] << 8) | (ent[30] << 16) | (ent[31] << 24);
                found_file = true;
                break;
            }
        }
        free(root_dir);

        if(!found_file || first_cluster < 2 || file_size == 0) {
            FURI_LOG_W(TAG, "passwords.txt not found in image or empty");
            free(fat);
            break;
        }

        if(file_size > 16384) file_size = 16384;

        char* file_content = malloc(file_size + 1);
        if(!file_content) {
            free(fat);
            break;
        }

        uint16_t curr_cluster = first_cluster;
        size_t bytes_loaded = 0;
        uint8_t* sec_buf = malloc(FAT12_SECTOR_SIZE);

        while(curr_cluster >= 2 && curr_cluster < 0x0FF8 && bytes_loaded < file_size) {
            uint32_t cluster_sec = data_start_sec + (curr_cluster - 2);
            if(!storage_file_seek(file, cluster_sec * FAT12_SECTOR_SIZE, true)) break;
            if(storage_file_read(file, sec_buf, FAT12_SECTOR_SIZE) != FAT12_SECTOR_SIZE) break;

            size_t take = file_size - bytes_loaded;
            if(take > FAT12_SECTOR_SIZE) take = FAT12_SECTOR_SIZE;
            memcpy(file_content + bytes_loaded, sec_buf, take);
            bytes_loaded += take;

            curr_cluster = fat12_get_cluster(fat, curr_cluster);
        }
        free(sec_buf);
        free(fat);
        file_content[bytes_loaded] = '\0';

        memset(vault, 0, sizeof(PasswordVault));
        vault->magic = VAULT_MAGIC;
        vault->version = VAULT_VERSION;

        const char* p = file_content;
        const char* end = file_content + bytes_loaded;
        while(p < end) {
            const char* line_start = p;
            while(p < end && *p != '\n') p++;
            size_t line_len = p - line_start;
            parse_line(line_start, line_len, vault);
            if(p < end && *p == '\n') p++;
        }

        free(file_content);
        success = true;
    } while(false);

    storage_file_close(file);
    storage_file_free(file);
    return success;
}
