// RFC 6238 Test Vector Verification for totp.h
// Compile:
// g++ -I. -I../hash-library -I../base32 ../hash-library/sha1.cpp ../hash-library/sha256.cpp ../hash-library/sha512.cpp test_totp.cpp  -o test_totp && ./test_totp

#define TOTP_IMPLEMENTATION
#define BASE32_IMPLEMENTATION
#include "totp.hpp"
#include <cstdio>
#include <cstring>
#include <cassert>

// RFC 6238 test secrets (different length per algorithm!)
// SHA1:   20 bytes  "12345678901234567890"
// SHA256: 32 bytes  "12345678901234567890123456789012"
// SHA512: 64 bytes  "1234567890123456789012345678901234567890123456789012345678901234"
static const uint8_t TEST_SECRET_SHA1[]   = "12345678901234567890";
static const size_t  TEST_SECRET_SHA1_LEN = 20;

static const uint8_t TEST_SECRET_SHA256[] = "12345678901234567890123456789012";
static const size_t  TEST_SECRET_SHA256_LEN = 32;

static const uint8_t TEST_SECRET_SHA512[] = "1234567890123456789012345678901234567890123456789012345678901234";
static const size_t  TEST_SECRET_SHA512_LEN = 64;

// RFC 6238 Appendix B Test Vectors (8-digit mode)
struct TestCase {
    int64_t  timestamp;
    uint32_t sha1_code;
    uint32_t sha256_code;
    uint32_t sha512_code;
};

static const TestCase TESTS[] = {
    { 59,          94287082u, 46119246u, 90693936u },
    { 1111111109,  7081804u,  68084774u, 25091201u },
    { 1111111111,  14050471u, 67062674u, 99943326u },
    { 1234567890,  89005924u, 91819424u, 93441116u },
    { 2000000000,  69279037u, 90698825u, 38618901u },
    { 20000000000, 65353130u, 77737706u, 47863826u },
};

#include "time.h"
// current time
void test_current_time() {
    int64_t now = time(NULL);
    uint32_t sha1 = totp_from_b32("YLTQTQK2V6WIP346",TOTP_ALGO_SHA1, 6, 30, now);
    char code_str[9];
    totp_code_str(sha1, 6, code_str);
    printf("Current TOTP code (SHA1 6-digit): %s\n", code_str);
}


int main() {
    int pass = 0, fail = 0;

    printf("RFC 6238 Test Vector Verification\n");
    printf("================================\n\n");

    for (int i = 0; i < 6; i++) {
        const TestCase& tc = TESTS[i];

        // SHA1
        uint32_t sha1 = totp_compute(TEST_SECRET_SHA1, TEST_SECRET_SHA1_LEN,
                                     TOTP_ALGO_SHA1, 8, 30, tc.timestamp);
        // SHA256
        uint32_t sha256 = totp_compute(TEST_SECRET_SHA256, TEST_SECRET_SHA256_LEN,
                                       TOTP_ALGO_SHA256, 8, 30, tc.timestamp);

        // SHA512
        uint32_t sha512 = totp_compute(TEST_SECRET_SHA512, TEST_SECRET_SHA512_LEN,
                                       TOTP_ALGO_SHA512, 8, 30, tc.timestamp);

        bool sha1_ok   = (sha1   == tc.sha1_code);
        bool sha256_ok = (sha256 == tc.sha256_code);
        bool sha512_ok = (sha512 == tc.sha512_code);

        printf("Timestamp: %lld\n", (long long)tc.timestamp);
        printf("  SHA1:   expected %08u  got %08u  %s\n",
               tc.sha1_code, sha1, sha1_ok ? "PASS" : "FAIL");
        printf("  SHA256: expected %08u  got %08u  %s\n",
               tc.sha256_code, sha256, sha256_ok ? "PASS" : "FAIL");
        printf("  SHA512: expected %08u  got %08u  %s\n",
               tc.sha512_code, sha512, sha512_ok ? "PASS" : "FAIL");

        if (sha1_ok) pass++; else fail++;
        if (sha256_ok) pass++; else fail++;
        if (sha512_ok) pass++; else fail++;
    }

    printf("\n================================\n");
    printf("Results: %d passed, %d failed\n", pass, fail);

    // Quick helper tests
    printf("\n--- Helper function tests ---\n");

    int remaining = totp_remaining(30, 100);  // 100 % 30 = 10, remaining = 30-10 = 20
    printf("totp_remaining(30, 100) = %d (expect 20)\n", remaining);

    int64_t step_ts = totp_step_timestamp(30, 100, -1);
    printf("totp_step_timestamp(30, 100, -1) = %lld (expect 60)\n", (long long)step_ts);

    step_ts = totp_step_timestamp(30, 100, 1);
    printf("totp_step_timestamp(30, 100, 1) = %lld (expect 120)\n", (long long)step_ts);

    char code_str[9];
    totp_code_str(482913, 6, code_str);
    printf("totp_code_str(482913, 6) = \"%s\" (expect \"482913\")\n", code_str);

    totp_code_str(7081804, 8, code_str);
    printf("totp_code_str(7081804, 8) = \"%s\" (expect \"07081804\")\n", code_str);

    int algo = totp_parse_algorithm("SHA256");
    printf("totp_parse_algorithm(\"SHA256\") = %d (expect %d)\n", algo, TOTP_ALGO_SHA256);

    test_current_time();

    return fail > 0 ? 1 : 0;
}
