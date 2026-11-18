#include "vault.h"
#include <furi.h>
#include <string.h>

void vault_derive_key(const uint8_t* arrows, size_t count, uint8_t key_out[32]) {
    UNUSED(arrows);
    UNUSED(count);
    if(key_out) memset(key_out, 0, 32);
}

void vault_encrypt(
    const PasswordVault* vault,
    const uint8_t key[32],
    const uint8_t nonce[12],
    uint8_t* out_ciphertext) {
    UNUSED(key);
    UNUSED(nonce);
    if(out_ciphertext && vault) {
        memcpy(out_ciphertext, vault, sizeof(PasswordVault));
    }
}

bool vault_decrypt(
    const uint8_t* in_ciphertext,
    size_t in_len,
    const uint8_t key[32],
    const uint8_t nonce[12],
    PasswordVault* out_vault) {
    UNUSED(key);
    UNUSED(nonce);
    if(in_len != sizeof(PasswordVault) || !in_ciphertext || !out_vault) {
        return false;
    }
    memcpy(out_vault, in_ciphertext, sizeof(PasswordVault));
    return (out_vault->magic == VAULT_MAGIC);
}

bool vault_file_exists(Storage* storage, const char* path) {
    return storage_file_exists(storage, path);
}

bool vault_save_file(
    Storage* storage,
    const char* path,
    const PasswordVault* vault,
    const uint8_t key[32]) {
    UNUSED(key);
    File* file = storage_file_alloc(storage);
    bool success = false;

    do {
        if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            break;
        }

        const uint8_t* p = (const uint8_t*)vault;
        size_t rem = sizeof(PasswordVault);
        success = true;
        while(rem > 0) {
            size_t w = storage_file_write(file, p, rem);
            if(w == 0) {
                success = false;
                break;
            }
            p += w;
            rem -= w;
        }
    } while(false);

    storage_file_close(file);
    storage_file_free(file);
    return success;
}

bool vault_load_file(
    Storage* storage,
    const char* path,
    const uint8_t key[32],
    PasswordVault* out_vault) {
    UNUSED(key);
    File* file = storage_file_alloc(storage);
    bool success = false;

    do {
        if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            break;
        }

        uint8_t* p = (uint8_t*)out_vault;
        size_t rem = sizeof(PasswordVault);
        success = true;
        while(rem > 0) {
            size_t r = storage_file_read(file, p, rem);
            if(r == 0) {
                success = false;
                break;
            }
            p += r;
            rem -= r;
        }
    } while(false);

    storage_file_close(file);
    storage_file_free(file);

    if(success) {
        if(out_vault->magic != VAULT_MAGIC) {
            memset(out_vault, 0, sizeof(PasswordVault));
            out_vault->magic = VAULT_MAGIC;
            out_vault->version = VAULT_VERSION;
        }
    }
    return success;
}
