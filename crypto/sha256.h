#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHA256_BLOCK_SIZE 64
#define SHA256_DIGEST_SIZE 32

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[SHA256_BLOCK_SIZE];
} Sha256Context;

/**
 * @brief Initialize SHA-256 context.
 *
 * @param ctx Context to initialize
 */
void sha256_init(Sha256Context* ctx);

/**
 * @brief Update SHA-256 context with new data.
 *
 * @param ctx SHA-256 context
 * @param data Data buffer to hash
 * @param len Length of data in bytes
 */
void sha256_update(Sha256Context* ctx, const uint8_t* data, size_t len);

/**
 * @brief Finalize SHA-256 computation and output 32-byte digest.
 *
 * Context is securely zeroed upon completion.
 *
 * @param ctx SHA-256 context
 * @param hash Output buffer (32 bytes)
 */
void sha256_final(Sha256Context* ctx, uint8_t hash[SHA256_DIGEST_SIZE]);

/**
 * @brief One-shot calculation of SHA-256 digest.
 *
 * @param data Data buffer to hash
 * @param len Length of data in bytes
 * @param hash Output buffer (32 bytes)
 */
void sha256_calc(const uint8_t* data, size_t len, uint8_t hash[SHA256_DIGEST_SIZE]);

#ifdef __cplusplus
}
#endif
