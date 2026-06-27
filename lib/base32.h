/**
 * base32.h
 * Base32 encoding/decoding (RFC 4648)
 *
 * STB-style single-header library.
 *
 * License: MIT
 */

#ifndef BASE32_H_INCLUDED
#define BASE32_H_INCLUDED

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t base32_encode(char *dest, const void *src, size_t src_len);
size_t base32_decode(void *dest, size_t dest_len, const char *src);
size_t base32_encsize(size_t src_len);
size_t base32_decsize(size_t enc_len);

#ifdef __cplusplus
}
#endif

/* ---- Implementation ------------------------------------------------------ */
#ifdef BASE32_IMPLEMENTATION

static const char base32_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
static const char base32_padding = '=';

static inline int base32_char_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '2' && c <= '7') return c - '2' + 26;
    return -1;
}

/* Encode 5 bytes -> 8 chars (no padding) */
static inline void base32_encode_block(char *dest, const uint8_t *src) {
    dest[0] = base32_alphabet[(src[0] >> 3) & 0x1F];
    dest[1] = base32_alphabet[((src[0] << 2) & 0x1C) | ((src[1] >> 6) & 0x03)];
    dest[2] = base32_alphabet[(src[1] >> 1) & 0x1F];
    dest[3] = base32_alphabet[((src[1] << 4) & 0x10) | ((src[2] >> 4) & 0x0F)];
    dest[4] = base32_alphabet[((src[2] << 1) & 0x1E) | ((src[3] >> 7) & 0x01)];
    dest[5] = base32_alphabet[(src[3] >> 2) & 0x1F];
    dest[6] = base32_alphabet[((src[3] << 3) & 0x18) | ((src[4] >> 5) & 0x07)];
    dest[7] = base32_alphabet[src[4] & 0x1F];
}

/* Encode last block (1-4 bytes) with padding */
static inline void base32_encode_tail(char *dest, const uint8_t *src, size_t len) {
    for (size_t i = 0; i < 8; i++) dest[i] = base32_padding;

    switch (len) {
        case 1:
            dest[0] = base32_alphabet[(src[0] >> 3) & 0x1F];
            dest[1] = base32_alphabet[(src[0] << 2) & 0x1C];
            break;
        case 2:
            dest[0] = base32_alphabet[(src[0] >> 3) & 0x1F];
            dest[1] = base32_alphabet[((src[0] << 2) & 0x1C) | ((src[1] >> 6) & 0x03)];
            dest[2] = base32_alphabet[(src[1] >> 1) & 0x1F];
            dest[3] = base32_alphabet[(src[1] << 4) & 0x10];
            break;
        case 3:
            dest[0] = base32_alphabet[(src[0] >> 3) & 0x1F];
            dest[1] = base32_alphabet[((src[0] << 2) & 0x1C) | ((src[1] >> 6) & 0x03)];
            dest[2] = base32_alphabet[(src[1] >> 1) & 0x1F];
            dest[3] = base32_alphabet[((src[1] << 4) & 0x10) | ((src[2] >> 4) & 0x0F)];
            dest[4] = base32_alphabet[(src[2] << 1) & 0x1E];
            break;
        case 4:
            dest[0] = base32_alphabet[(src[0] >> 3) & 0x1F];
            dest[1] = base32_alphabet[((src[0] << 2) & 0x1C) | ((src[1] >> 6) & 0x03)];
            dest[2] = base32_alphabet[(src[1] >> 1) & 0x1F];
            dest[3] = base32_alphabet[((src[1] << 4) & 0x10) | ((src[2] >> 4) & 0x0F)];
            dest[4] = base32_alphabet[((src[2] << 1) & 0x1E) | ((src[3] >> 7) & 0x01)];
            dest[5] = base32_alphabet[(src[3] >> 2) & 0x1F];
            dest[6] = base32_alphabet[(src[3] << 3) & 0x18];
            break;
        default: break;
    }
}

/// @brief  Base32 encode a byte buffer. Output is null-terminated string.  
/// @param dest  Output buffer (must have space for base32_encsize(src_len) + 1 bytes)
/// @param src   Input byte buffer
/// @param src_len  Length of input buffer in bytes
/// @return  Number of characters written (excluding null terminator)
size_t base32_encode(char *dest, const void *src, size_t src_len) {
    const uint8_t *bytes = (const uint8_t *)src;
    size_t out_len = 0;
    size_t i = 0;

    while (src_len - i >= 5) {
        base32_encode_block(dest + out_len, bytes + i);
        out_len += 8;
        i += 5;
    }
    if (src_len - i > 0) {
        base32_encode_tail(dest + out_len, bytes + i, src_len - i);
        out_len += 8;
    }
    dest[out_len] = '\0';
    return out_len;
}

/* Decode 8 chars -> up to 5 bytes, handling padding.
   Returns number of bytes written (0..5), or (size_t)-1 on error. */
static inline size_t base32_decode_block(uint8_t *dest, const char *src) {
    int values[8];
    int padding = 0;
    size_t i;

    /* Convert chars to 5-bit values, check padding */
    for (i = 0; i < 8; i++) {
        char c = src[i];
        if (c == base32_padding) {
            padding++;
            values[i] = 0;
        } else {
            if (padding != 0) return (size_t)-1;
            int v = base32_char_value(c);
            if (v < 0) return (size_t)-1;
            values[i] = v;
        }
    }

    /* Determine output length from padding count */
    size_t out_len;
    switch (padding) {
        case 0: out_len = 5; break;
        case 1: out_len = 4; break;
        case 3: out_len = 3; break;
        case 4: out_len = 2; break;
        case 6: out_len = 1; break;
        default: return (size_t)-1;
    }

    /* Decode using bit shifting (RFC 4648 direct method) */
    dest[0] = (uint8_t)((values[0] << 3) | (values[1] >> 2));
    if (out_len == 1) return 1;

    dest[1] = (uint8_t)(((values[1] & 0x03) << 6) | (values[2] << 1) | (values[3] >> 4));
    if (out_len == 2) return 2;

    dest[2] = (uint8_t)(((values[3] & 0x0F) << 4) | (values[4] >> 1));
    if (out_len == 3) return 3;

    dest[3] = (uint8_t)(((values[4] & 0x01) << 7) | (values[5] << 2) | (values[6] >> 3));
    if (out_len == 4) return 4;

    dest[4] = (uint8_t)(((values[6] & 0x07) << 5) | values[7]);
    return 5;
}

/// @brief      Base32 decode a string. Input must be a valid Base32 string with correct padding.
/// @param dest Output buffer (must have space for base32_decsize(enc_len) bytes)
/// @param dest_len Size of output buffer in bytes
/// @param src  Null-terminated Base32 string to decode
/// @return     Number of bytes written to dest, or (size_t)-1 on error (invalid input or insufficient dest_len)
size_t base32_decode(void *dest, size_t dest_len, const char *src) {
    uint8_t *out = (uint8_t *)dest;
    size_t src_len = strlen(src);
    size_t out_pos = 0;
    size_t i = 0;

    /* Input length must be a multiple of 8 (RFC 4648) */
    if (src_len % 8 != 0) return (size_t)-1;

    while (i < src_len) {
        uint8_t block[5];
        size_t n = base32_decode_block(block, src + i);
        if (n == (size_t)-1) return (size_t)-1;
        if (out_pos + n > dest_len) return (size_t)-1;
        memcpy(out + out_pos, block, n);
        out_pos += n;
        i += 8;
    }
    return out_pos;
}

// Calculate encoded length for given input length (without null terminator)
size_t base32_encsize(size_t src_len) {
    return ((src_len + 4) / 5) * 8;
}

// Calculate decoded length for given encoded length (must be multiple of 8)
size_t base32_decsize(size_t enc_len) {
    return (enc_len / 8) * 5;
}

#endif /* BASE32_IMPLEMENTATION */
#endif /* BASE32_H_INCLUDED */