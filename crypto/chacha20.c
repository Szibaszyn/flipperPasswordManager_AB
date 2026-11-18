#include "chacha20.h"
#include <string.h>

#define ROTL32(v, n) (((v) << (n)) | ((v) >> (32 - (n))))

#define QR(a, b, c, d)           \
    do {                         \
        a += b; d ^= a; d = ROTL32(d, 16); \
        c += d; b ^= c; b = ROTL32(b, 12); \
        a += b; d ^= a; d = ROTL32(d, 8);  \
        c += d; b ^= c; b = ROTL32(b, 7);  \
    } while(0)

static uint32_t load32_le(const uint8_t* p) {
    return ((uint32_t)p[0]) |
           (((uint32_t)p[1]) << 8) |
           (((uint32_t)p[2]) << 16) |
           (((uint32_t)p[3]) << 24);
}

static void store32_le(uint8_t* p, uint32_t val) {
    p[0] = (uint8_t)(val);
    p[1] = (uint8_t)(val >> 8);
    p[2] = (uint8_t)(val >> 16);
    p[3] = (uint8_t)(val >> 24);
}

static void chacha20_block(const uint32_t in[16], uint8_t out[64]) {
    uint32_t x[16];
    memcpy(x, in, sizeof(x));

    for(int i = 0; i < 10; i++) {
        // Column rounds
        QR(x[0], x[4], x[8],  x[12]);
        QR(x[1], x[5], x[9],  x[13]);
        QR(x[2], x[6], x[10], x[14]);
        QR(x[3], x[7], x[11], x[15]);
        // Diagonal rounds
        QR(x[0], x[5], x[10], x[15]);
        QR(x[1], x[6], x[11], x[12]);
        QR(x[2], x[7], x[8],  x[13]);
        QR(x[3], x[4], x[9],  x[14]);
    }

    for(int i = 0; i < 16; i++) {
        store32_le(out + i * 4, x[i] + in[i]);
    }
}

void chacha20_xor(
    const uint8_t key[CHACHA20_KEY_SIZE],
    const uint8_t nonce[CHACHA20_NONCE_SIZE],
    uint32_t counter,
    const uint8_t* in,
    uint8_t* out,
    size_t len) {
    uint32_t state[16];

    // "expand 32-byte k" constants
    state[0] = 0x61707865;
    state[1] = 0x3320646e;
    state[2] = 0x79622d32;
    state[3] = 0x6b206574;

    for(int i = 0; i < 8; i++) {
        state[4 + i] = load32_le(key + i * 4);
    }

    state[12] = counter;
    state[13] = load32_le(nonce + 0);
    state[14] = load32_le(nonce + 4);
    state[15] = load32_le(nonce + 8);

    uint8_t key_stream[64];
    size_t offset = 0;

    while(offset < len) {
        state[12] = counter++;
        chacha20_block(state, key_stream);

        size_t block_take = (len - offset < 64) ? (len - offset) : 64;
        for(size_t i = 0; i < block_take; i++) {
            out[offset + i] = in[offset + i] ^ key_stream[i];
        }
        offset += block_take;
    }
}
