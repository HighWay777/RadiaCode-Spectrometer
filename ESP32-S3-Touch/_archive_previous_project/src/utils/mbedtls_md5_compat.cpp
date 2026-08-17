// Compatibility shim for ESP Async WebServer
// Provides mbedtls_md5_starts/update/finish using a minimal built-in MD5
// implementation compatible with mbedtls_md5_context. This avoids dependency
// on *_ret symbols that may not be present in some Arduino-ESP32 builds.

extern "C" {
#include "mbedtls/md5.h"
}

#ifdef mbedtls_md5_starts
#undef mbedtls_md5_starts
#endif
#ifdef mbedtls_md5_update
#undef mbedtls_md5_update
#endif
#ifdef mbedtls_md5_finish
#undef mbedtls_md5_finish
#endif

#include <string.h>
#include <stdint.h>

// Left-rotate a 32-bit value
static inline uint32_t MD5_ROTL(uint32_t x, uint32_t n) {
    return (x << n) | (x >> (32U - n));
}

// Process a single 64-byte block
static void md5_process_block(mbedtls_md5_context *ctx, const unsigned char data[64]) {
    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];

    uint32_t X[16];
    for (int i = 0; i < 16; ++i) {
        X[i] = (uint32_t)data[i*4] | ((uint32_t)data[i*4 + 1] << 8) |
               ((uint32_t)data[i*4 + 2] << 16) | ((uint32_t)data[i*4 + 3] << 24);
    }

    // Round functions
#define F(x,y,z) (((x) & (y)) | (~(x) & (z)))
#define G(x,y,z) (((x) & (z)) | ((y) & ~(z)))
#define H(x,y,z) ((x) ^ (y) ^ (z))
#define I(x,y,z) ((y) ^ ((x) | ~(z)))

#define STEP(f, a, b, c, d, x, t, s) \
    do { \
        (a) += f((b), (c), (d)) + (x) + (uint32_t)(t); \
        (a) = MD5_ROTL((a), (s)); \
        (a) += (b); \
    } while (0)

    // Round 1
    STEP(F, a, b, c, d, X[ 0], 0xd76aa478,  7);
    STEP(F, d, a, b, c, X[ 1], 0xe8c7b756, 12);
    STEP(F, c, d, a, b, X[ 2], 0x242070db, 17);
    STEP(F, b, c, d, a, X[ 3], 0xc1bdceee, 22);
    STEP(F, a, b, c, d, X[ 4], 0xf57c0faf,  7);
    STEP(F, d, a, b, c, X[ 5], 0x4787c62a, 12);
    STEP(F, c, d, a, b, X[ 6], 0xa8304613, 17);
    STEP(F, b, c, d, a, X[ 7], 0xfd469501, 22);
    STEP(F, a, b, c, d, X[ 8], 0x698098d8,  7);
    STEP(F, d, a, b, c, X[ 9], 0x8b44f7af, 12);
    STEP(F, c, d, a, b, X[10], 0xffff5bb1, 17);
    STEP(F, b, c, d, a, X[11], 0x895cd7be, 22);
    STEP(F, a, b, c, d, X[12], 0x6b901122,  7);
    STEP(F, d, a, b, c, X[13], 0xfd987193, 12);
    STEP(F, c, d, a, b, X[14], 0xa679438e, 17);
    STEP(F, b, c, d, a, X[15], 0x49b40821, 22);

    // Round 2
    STEP(G, a, b, c, d, X[ 1], 0xf61e2562,  5);
    STEP(G, d, a, b, c, X[ 6], 0xc040b340,  9);
    STEP(G, c, d, a, b, X[11], 0x265e5a51, 14);
    STEP(G, b, c, d, a, X[ 0], 0xe9b6c7aa, 20);
    STEP(G, a, b, c, d, X[ 5], 0xd62f105d,  5);
    STEP(G, d, a, b, c, X[10],  0x2441453,  9);
    STEP(G, c, d, a, b, X[15], 0xd8a1e681, 14);
    STEP(G, b, c, d, a, X[ 4], 0xe7d3fbc8, 20);
    STEP(G, a, b, c, d, X[ 9], 0x21e1cde6,  5);
    STEP(G, d, a, b, c, X[14], 0xc33707d6,  9);
    STEP(G, c, d, a, b, X[ 3], 0xf4d50d87, 14);
    STEP(G, b, c, d, a, X[ 8], 0x455a14ed, 20);
    STEP(G, a, b, c, d, X[13], 0xa9e3e905,  5);
    STEP(G, d, a, b, c, X[ 2], 0xfcefa3f8,  9);
    STEP(G, c, d, a, b, X[ 7], 0x676f02d9, 14);
    STEP(G, b, c, d, a, X[12], 0x8d2a4c8a, 20);

    // Round 3
    STEP(H, a, b, c, d, X[ 5], 0xfffa3942,  4);
    STEP(H, d, a, b, c, X[ 8], 0x8771f681, 11);
    STEP(H, c, d, a, b, X[11], 0x6d9d6122, 16);
    STEP(H, b, c, d, a, X[14], 0xfde5380c, 23);
    STEP(H, a, b, c, d, X[ 1], 0xa4beea44,  4);
    STEP(H, d, a, b, c, X[ 4], 0x4bdecfa9, 11);
    STEP(H, c, d, a, b, X[ 7], 0xf6bb4b60, 16);
    STEP(H, b, c, d, a, X[10], 0xbebfbc70, 23);
    STEP(H, a, b, c, d, X[13], 0x289b7ec6,  4);
    STEP(H, d, a, b, c, X[ 0], 0xeaa127fa, 11);
    STEP(H, c, d, a, b, X[ 3], 0xd4ef3085, 16);
    STEP(H, b, c, d, a, X[ 6], 0x04881d05, 23);
    STEP(H, a, b, c, d, X[ 9], 0xd9d4d039,  4);
    STEP(H, d, a, b, c, X[12], 0xe6db99e5, 11);
    STEP(H, c, d, a, b, X[15], 0x1fa27cf8, 16);
    STEP(H, b, c, d, a, X[ 2], 0xc4ac5665, 23);

    // Round 4
    STEP(I, a, b, c, d, X[ 0], 0xf4292244,  6);
    STEP(I, d, a, b, c, X[ 7], 0x432aff97, 10);
    STEP(I, c, d, a, b, X[14], 0xab9423a7, 15);
    STEP(I, b, c, d, a, X[ 5], 0xfc93a039, 21);
    STEP(I, a, b, c, d, X[12], 0x655b59c3,  6);
    STEP(I, d, a, b, c, X[ 3], 0x8f0ccc92, 10);
    STEP(I, c, d, a, b, X[10], 0xffeff47d, 15);
    STEP(I, b, c, d, a, X[ 1], 0x85845dd1, 21);
    STEP(I, a, b, c, d, X[ 8], 0x6fa87e4f,  6);
    STEP(I, d, a, b, c, X[15], 0xfe2ce6e0, 10);
    STEP(I, c, d, a, b, X[ 6], 0xa3014314, 15);
    STEP(I, b, c, d, a, X[13], 0x4e0811a1, 21);
    STEP(I, a, b, c, d, X[ 4], 0xf7537e82,  6);
    STEP(I, d, a, b, c, X[11], 0xbd3af235, 10);
    STEP(I, c, d, a, b, X[ 2], 0x2ad7d2bb, 15);
    STEP(I, b, c, d, a, X[ 9], 0xeb86d391, 21);

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;

#undef F
#undef G
#undef H
#undef I
#undef STEP
}

extern "C" void mbedtls_md5_starts(mbedtls_md5_context *ctx) {
    ctx->total[0] = 0;
    ctx->total[1] = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

extern "C" void mbedtls_md5_update(mbedtls_md5_context *ctx, const unsigned char *input, size_t ilen) {
    if (ilen == 0) return;

    uint32_t left = ctx->total[0] & 0x3F; // bytes in buffer
    uint32_t fill = 64 - left;

    ctx->total[0] += (uint32_t)ilen;
    if (ctx->total[0] < (uint32_t)ilen) ctx->total[1]++;

    size_t offset = 0;
    if (left && ilen >= fill) {
        memcpy(ctx->buffer + left, input, fill);
        md5_process_block(ctx, ctx->buffer);
        offset += fill;
        left = 0;
    }

    while (offset + 64 <= ilen) {
        md5_process_block(ctx, input + offset);
        offset += 64;
    }

    if (offset < ilen) {
        memcpy(ctx->buffer + left, input + offset, ilen - offset);
    }
}

extern "C" void mbedtls_md5_finish(mbedtls_md5_context *ctx, unsigned char output[16]) {
    static const unsigned char pad[64] = { 0x80 };

    unsigned char msglen[8];
    uint32_t low = (ctx->total[0] << 3);
    uint32_t high = (ctx->total[1] << 3) | (ctx->total[0] >> 29);

    // Length in bits, little-endian
    msglen[0] = (unsigned char)(low);
    msglen[1] = (unsigned char)(low >> 8);
    msglen[2] = (unsigned char)(low >> 16);
    msglen[3] = (unsigned char)(low >> 24);
    msglen[4] = (unsigned char)(high);
    msglen[5] = (unsigned char)(high >> 8);
    msglen[6] = (unsigned char)(high >> 16);
    msglen[7] = (unsigned char)(high >> 24);

    // Pad with a single 1 bit then zeros, then append length
    uint32_t last = ctx->total[0] & 0x3F;
    uint32_t padn = (last < 56) ? (56 - last) : (120 - last);
    mbedtls_md5_update(ctx, pad, padn);
    mbedtls_md5_update(ctx, msglen, 8);

    // Output digest (little-endian)
    for (int i = 0; i < 4; ++i) {
        output[i * 4 + 0] = (unsigned char)(ctx->state[i] & 0xFF);
        output[i * 4 + 1] = (unsigned char)((ctx->state[i] >> 8) & 0xFF);
        output[i * 4 + 2] = (unsigned char)((ctx->state[i] >> 16) & 0xFF);
        output[i * 4 + 3] = (unsigned char)((ctx->state[i] >> 24) & 0xFF);
    }
}

// Also provide *_ret variants expected by some IDF versions/macros
extern "C" int mbedtls_md5_starts_ret(mbedtls_md5_context *ctx) {
    mbedtls_md5_starts(ctx);
    return 0;
}

extern "C" int mbedtls_md5_update_ret(mbedtls_md5_context *ctx, const unsigned char *input, size_t ilen) {
    mbedtls_md5_update(ctx, input, ilen);
    return 0;
}

extern "C" int mbedtls_md5_finish_ret(mbedtls_md5_context *ctx, unsigned char output[16]) {
    mbedtls_md5_finish(ctx, output);
    return 0;
}
