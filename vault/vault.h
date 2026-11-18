#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <storage/storage.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ArrowUp = 0,    // 00
    ArrowDown = 1,  // 01
    ArrowLeft = 2,  // 10
    ArrowRight = 3, // 11
} ArrowDirection;

#define VAULT_MAGIC         0x4650574D // 'FPWM'
#define VAULT_VERSION       1
#define MAX_PASSWORD_LEN    64
#define MAX_DESCRIPTION_LEN 64
#define MAX_VAULT_ENTRIES   64
#define MAX_ARROW_COMBO     64

typedef struct {
    char password[MAX_PASSWORD_LEN];
    char description[MAX_DESCRIPTION_LEN];
} PasswordEntry;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t entry_count;
    uint32_t checksum;
    PasswordEntry entries[MAX_VAULT_ENTRIES];
} PasswordVault;

/**
 * @brief Derives a 32-byte ChaCha20 key from an arrow combo.
 *
 * Each arrow is mapped to 2 bits (00, 01, 10, 11).
 * The bitstream and combo length are hashed with SHA-256.
 */
void vault_derive_key(const uint8_t* arrows, size_t count, uint8_t key_out[32]);

/**
 * @brief Encrypts a PasswordVault structure using ChaCha20.
 *
 * Computes CRC32 checksum of entries and encrypts the structure.
 */
void vault_encrypt(
    const PasswordVault* vault,
    const uint8_t key[32],
    const uint8_t nonce[12],
    uint8_t* out_ciphertext);

/**
 * @brief Decrypts ciphertext into a PasswordVault and validates integrity.
 *
 * @return true if magic and checksum match, false if wrong key or corrupted
 */
bool vault_decrypt(
    const uint8_t* in_ciphertext,
    size_t in_len,
    const uint8_t key[32],
    const uint8_t nonce[12],
    PasswordVault* out_vault);

/**
 * @brief Saves vault to encrypted file on storage.
 */
bool vault_save_file(
    Storage* storage,
    const char* path,
    const PasswordVault* vault,
    const uint8_t key[32]);

/**
 * @brief Loads and decrypts vault from file on storage.
 */
bool vault_load_file(
    Storage* storage,
    const char* path,
    const uint8_t key[32],
    PasswordVault* out_vault);

/**
 * @brief Checks if a vault file exists on storage.
 */
bool vault_file_exists(Storage* storage, const char* path);

#ifdef __cplusplus
}
#endif
