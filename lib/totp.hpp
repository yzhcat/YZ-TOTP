// totp.hpp — TOTP: Time-Based One-Time Password (RFC 6238)
// Pure C++17 header-only library.
// Dependencies: hash-library (sha1.h, sha256.h, hmac.h, etc.)
// Optional: base32.h (for totp_from_b32)
//
// Usage:
//   #include "totp.hpp"
//   // ... use totp_compute() or totp_from_b32() directly
//
//   // Compute from raw key bytes (already base32-decoded):
//   uint32_t code = totp_compute(secret_raw, secret_len,
//                                TOTP_ALGO_SHA1, 6, 30, unix_ts);
//
//   // Compute from base32 string (convenience, needs base32.h):
//   uint32_t code = totp_from_b32("JBSWY3DPEHPK3PXP",
//                                 TOTP_ALGO_SHA1, 6, 30, unix_ts);
//
//   // Format as zero-padded string:
//   char buf[9];
//   totp_code_str(code, 6, buf);  // "482913"
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>


// ─── Algorithm enum ───────────────────────────────────────

typedef enum {
    TOTP_ALGO_SHA1   = 0,
    TOTP_ALGO_SHA256 = 1,
    TOTP_ALGO_SHA512 = 2,
    TOTP_ALGO_COUNT  = 3,
} TOTPAlgorithm;

/// Parse algorithm name string → enum.
/// Recognizes: "SHA1", "SHA256", "SHA512" (case-sensitive per spec).
/// Returns -1 on unrecognized input.
inline int totp_parse_algorithm(const char* name){
    if (!name) return TOTP_ALGO_SHA1;  // default per spec
    if (strcmp(name, "SHA1")   == 0) return TOTP_ALGO_SHA1;
    if (strcmp(name, "SHA256") == 0) return TOTP_ALGO_SHA256;
    if (strcmp(name, "SHA512") == 0) return TOTP_ALGO_SHA512;
    return -1;
}


// ── hash-library headers ──────────────────────────────────
// hash-library is C++ only; totp.h implementation is C++ too.
// Compile with: g++ -std=c++17 ... sha1.cpp sha256.cpp [sha512.cpp]
#include "hash-library/sha1.h"
#include "hash-library/sha256.h"
#include "hash-library/sha512.h"
#include "hash-library/hmac.h"
// Maximum hash output: SHA512 = 64 bytes
#define TOTP_MAX_HASH_BYTES_ 64

// ── Internal: hex string → raw bytes ──────────────────────
// hash-library's hmac<>() returns a hex string.
// TOTP dynamic truncation needs raw bytes, so we convert back.
namespace {
    int totp_hexval_(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    int totp_hex_to_bytes_(const std::string& hex,
                                  uint8_t* out, size_t out_size) {
        size_t len = hex.size();
        if (len % 2 != 0) return -1;
        size_t byte_count = len / 2;
        if (byte_count > out_size) return -1;
        for (size_t i = 0; i < byte_count; i++) {
            int hi = totp_hexval_(hex[2 * i]);
            int lo = totp_hexval_(hex[2 * i + 1]);
            if (hi < 0 || lo < 0) return -1;
            out[i] = (uint8_t)((hi << 4) | lo);
        }
        return (int)byte_count;
    }
    // ── Internal: HOTP (RFC 4226) ─────────────────────────────
    // HOTP(K, C) = Truncate(HMAC-SHA(K, C)) mod 10^Digit
    
    uint32_t totp_hotp_(const void* secret, size_t secret_len,
                               TOTPAlgorithm algo, int digits,
                               const uint8_t* counter, size_t counter_len) {
        // Compute HMAC — hash-library returns hex string
        std::string hmac_hex;
    
        switch (algo) {
        case TOTP_ALGO_SHA1:
            hmac_hex = hmac<SHA1>(
                (const char*)counter, counter_len,
                (const char*)secret, secret_len);
            break;
        case TOTP_ALGO_SHA256:
            hmac_hex = hmac<SHA256>(
                (const char*)counter, counter_len,
                (const char*)secret, secret_len);
            break;
        case TOTP_ALGO_SHA512:
            // Requires JusticeRage/hash-library fork with sha512.h
            hmac_hex = hmac<SHA512>(
                (const char*)counter, counter_len,
                (const char*)secret, secret_len);
            break;
        default:
            return 0;
        }
    
        // Convert hex → raw bytes
        uint8_t hmac_bytes[TOTP_MAX_HASH_BYTES_];
        int hmac_len = totp_hex_to_bytes_(hmac_hex, hmac_bytes, TOTP_MAX_HASH_BYTES_);
        if (hmac_len <= 0) return 0;
    
        // ── Dynamic Truncation (RFC 4226 §5.3) ───────────
        int offset = hmac_bytes[hmac_len - 1] & 0x0F;
    
        uint32_t binary =
            ((uint32_t)(hmac_bytes[offset]     & 0x7F) << 24) |
            ((uint32_t)(hmac_bytes[offset + 1] & 0xFF) << 16) |
            ((uint32_t)(hmac_bytes[offset + 2] & 0xFF) <<  8) |
            ((uint32_t)(hmac_bytes[offset + 3] & 0xFF));
    
        // Modulo 10^digits
        static const uint32_t TOTP_POW10_[] = {
            1, 10, 100, 1000, 10000,
            100000, 1000000, 10000000, 100000000
        };
        if (digits < 1 || digits > 8) digits = 6;
        return binary % TOTP_POW10_[digits];
    }
}

// ─── Core TOTP computation ────────────────────────────────


/// @brief  Compute TOTP code from raw key bytes.
/// @param secret       pointer to raw key bytes (after base32 decode)
/// @param secret_len   length of secret in bytes
/// @param algo         HMAC algorithm (TOTP_ALGO_SHA1 / SHA256 / SHA512)
/// @param digits       code length (6 or 8, default 6)
/// @param period       time step in seconds (default 30)
/// @param timestamp    Unix timestamp (seconds since epoch)
/// @return             TOTP code as integer (e.g. 482913 for 6-digit), or 0 on error (invalid algo or digits)
inline uint32_t totp_compute(const void* secret, size_t secret_len,
                      TOTPAlgorithm algo, int digits,
                      int period, int64_t timestamp) {

// ── TOTP computation (RFC 6238) ────────────────────────────
// T = floor((timestamp - T0) / X)  where T0=0, X=period
// TOTP(K, T) = HOTP(K, T)
    if (period <= 0) period = 30;
    if (digits <= 0) digits = 6;

    // Time step counter
    uint64_t T = (uint64_t)(timestamp / (int64_t)period);

    // Encode T as big-endian 8-byte integer
    uint8_t counter[8];
    counter[0] = (uint8_t)(T >> 56);
    counter[1] = (uint8_t)(T >> 48);
    counter[2] = (uint8_t)(T >> 40);
    counter[3] = (uint8_t)(T >> 32);
    counter[4] = (uint8_t)(T >> 24);
    counter[5] = (uint8_t)(T >> 16);
    counter[6] = (uint8_t)(T >>  8);
    counter[7] = (uint8_t)(T);

    return totp_hotp_(secret, secret_len, algo, digits, counter, 8);
}

// 条件提供 base32 版本
#if __has_include("base32.h")
extern "C" {
#define BASE32_IMPLEMENTATION
#include "base32.h"
}
// ─── Convenience: base32 wrapper ──────────────────────────

/// @brief - Compute TOTP from a Base32-encoded secret string.
/// @brief - Requires base32.h. Disable with #define TOTP_NO_BASE32.
/// @param secret_b32 Base32-encoded secret (no padding required)
/// @param algo HMAC algorithm
/// @param digits Code length (6 or 8)
/// @param period Time step in seconds
/// @param timestamp Unix timestamp
/// @return TOTP code as integer, or 0 on error (invalid base32 input)
inline uint32_t totp_from_b32(const char* secret_b32,
                              TOTPAlgorithm algo, int digits,
                              int period, int64_t timestamp) {
    if (!secret_b32) return 0;

    size_t dec_len = base32_decsize(strlen(secret_b32));
    char *secret = (char*)malloc(dec_len);
    size_t secret_len = base32_decode(secret, dec_len, secret_b32);

    uint32_t code = totp_compute(secret, secret_len, algo, digits, period, timestamp);
    free(secret);
    return code;
}
#endif

// ─── Helper functions ─────────────────────────────────────

/// Format TOTP code as zero-padded decimal string.
///   out:  buffer of at least (digits + 1) bytes
inline void totp_code_str(uint32_t code, int digits, char* out) {
    if (digits < 1 || digits > 8) digits = 6;
    static const char* FMT[] = {
        "", "%01u", "%02u", "%03u", "%04u",
        "%05u", "%06u", "%07u", "%08u"
    };
    snprintf(out, (size_t)(digits + 1), FMT[digits], code);
}

/// Seconds remaining in current time step.
/// Returns value in [0, period-1].
inline int totp_remaining(int period, int64_t timestamp) {
    if (period <= 0) period = 30;
    return (int)(period - (timestamp % period));
}

/// Unix timestamp at the start of a given time-step offset.
///   offset: periods to shift (neg=past, pos=future)
///   Example: totp_step_timestamp(30, now, -1) → start of previous step
inline int64_t totp_step_timestamp(int period, int64_t timestamp, int offset) {
    if (period <= 0) period = 30;
    int64_t current_step = timestamp / period;
    return (current_step + offset) * period;
}




