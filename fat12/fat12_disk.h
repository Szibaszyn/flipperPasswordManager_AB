#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <storage/storage.h>
#include "../vault/vault.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FAT12_SECTOR_SIZE       512
#define FAT12_TOTAL_SECTORS     128 // 64 KB image (128 * 512)
#define FAT12_SECTORS_PER_FAT   1
#define FAT12_ROOT_ENTRIES      32
#define FAT12_ROOT_SECTORS      2
#define FAT12_FIRST_DATA_SECTOR (1 + 2 * FAT12_SECTORS_PER_FAT + FAT12_ROOT_SECTORS) // 5

bool fat12_disk_create(
    const char* img_path,
    const PasswordVault* vault,
    Storage* storage);

bool fat12_disk_read_vault(
    const char* img_path,
    PasswordVault* vault,
    Storage* storage);

#ifdef __cplusplus
}
#endif
