#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHACHA20_KEY_SIZE   32
#define CHACHA20_NONCE_SIZE 12
#define CHACHA20_BLOCK_SIZE 64

/**
 * @brief XORs buffer with ChaCha20 keystream.
 *
 * @param key 32-byte key
 * @param nonce 12-byte nonce
 * @param counter Initial block counter (usually 1 or 0)
 * @param in Input buffer
 * @param out Output buffer (can be same as in for in-place)
 * @param len Buffer length
 */
void chacha20_xor(
    const uint8_t key[CHACHA20_KEY_SIZE],
    const uint8_t nonce[CHACHA20_NONCE_SIZE],
    uint32_t counter,
    const uint8_t* in,
    uint8_t* out,
    size_t len);

#ifdef __cplusplus
}
#endif
