// gcc test_otpauth.c -o test_otpauth && ./test_otpauth 
#define OTPAUTH_IMPLEMENTATION
#include "../otpauth.h"
#include <stdio.h>
#include <string.h>

void print_entry(const OTPAuthEntry *entry) {
    printf("  type: %s\n", entry->type ? entry->type : "(null)");
    printf("  issuer: %s\n", entry->issuer ? entry->issuer : "(null)");
    printf("  account: %s\n", entry->account ? entry->account : "(null)");
    printf("  secret_b32: %s\n", entry->secret_b32 ? entry->secret_b32 : "(null)");
    printf("  algorithm: %s\n", entry->algorithm ? entry->algorithm : "(null)");
    printf("  digits: %d\n", entry->digits);
    printf("  period: %d\n", entry->period);
    printf("  counter: %d\n", entry->counter);
}

void test_parse_serialize(const char *uri, const char *test_name) {
    printf("\n=== Test: %s ===\n", test_name);
    printf("Input URI: %s\n", uri);

    OTPAuthEntry entry;
    otpauth_init(&entry);

    if (!otpauth_parse(uri, &entry)) {
        printf("ERROR: Failed to parse URI\n");
        return;
    }

    printf("Parsed entry:\n");
    print_entry(&entry);

    size_t required_len = otpauth_serialize_len(&entry);
    printf("\nRequired buffer size: %zu\n", required_len);

    if (required_len == 0) {
        printf("ERROR: Failed to calculate buffer size\n");
        otpauth_free(&entry);
        return;
    }

    char *buf = (char *)malloc(required_len);
    if (!buf) {
        printf("ERROR: Failed to allocate buffer\n");
        otpauth_free(&entry);
        return;
    }

    if (!otpauth_serialize(&entry, buf, required_len)) {
        printf("ERROR: Failed to serialize\n");
        free(buf);
        otpauth_free(&entry);
        return;
    }

    printf("Serialized URI: %s\n", buf);

    OTPAuthEntry entry2;
    otpauth_init(&entry2);

    if (otpauth_parse(buf, &entry2)) {
        printf("\nRe-parsed entry:\n");
        print_entry(&entry2);

        if (strcmp(entry.type, entry2.type) == 0 &&
            strcmp(entry.secret_b32, entry2.secret_b32) == 0 &&
            strcmp(entry.algorithm, entry2.algorithm) == 0 &&
            entry.digits == entry2.digits &&
            entry.period == entry2.period) {
            printf("\nY Round-trip test PASSED\n");
        } else {
            printf("\nN Round-trip test FAILED\n");
        }
        otpauth_free(&entry2);
    } else {
        printf("ERROR: Failed to re-parse serialized URI\n");
    }

    free(buf);
    otpauth_free(&entry);
}

void test_edge_cases() {
    printf("\n=== Testing Edge Cases ===\n");

    OTPAuthEntry entry;
    otpauth_init(&entry);

    printf("\n1. Testing with NULL pointer:\n");
    if (!otpauth_parse(NULL, &entry)) {
        printf("Y Correctly rejected NULL URI\n");
    }

    printf("\n2. Testing with invalid scheme:\n");
    if (!otpauth_parse("http://example.com", &entry)) {
        printf("Y Correctly rejected invalid scheme\n");
    }

    printf("\n3. Testing with missing secret:\n");
    if (!otpauth_parse("otpauth://totp/test@example.com?issuer=Test", &entry)) {
        printf("Y Correctly rejected missing secret\n");
    }

    printf("\n4. Testing buffer size calculation:\n");
    const char *uri = "otpauth://totp/test@example.com?secret=ABCDEF";
    if (otpauth_parse(uri, &entry)) {
        size_t len = otpauth_serialize_len(&entry);
        printf("Required buffer size: %zu\n", len);

        char small_buf[10];
        if (!otpauth_serialize(&entry, small_buf, sizeof(small_buf))) {
            printf("Y Correctly rejected too small buffer\n");
        }

        char *proper_buf = (char *)malloc(len);
        if (proper_buf && otpauth_serialize(&entry, proper_buf, len)) {
            printf("Y Successfully serialized with exact buffer size\n");
            free(proper_buf);
        }
        otpauth_free(&entry);
    }
}

int main() {
    printf("OTPAuth Library Test Suite\n");
    printf("==========================\n");

    test_parse_serialize(
        "otpauth://totp/john.doe@email.com?secret=HXDMVJECJJWSRB3HWIZR4IFUGFTMXBOZ&issuer=ACME%20Co&algorithm=SHA256&digits=8&period=30",
        "Full TOTP URI with all parameters"
    );

    test_parse_serialize(
        "otpauth://totp/john.doe@email.com?secret=HXDMVJECJJWSRB3HWIZR4IFUGFTMXBOZ",
        "Minimal TOTP URI"
    );

    test_parse_serialize(
        "otpauth://hotp/alice@example.com?secret=ABCDEFGH&counter=10&digits=8",
        "HOTP URI with counter"
    );

    test_parse_serialize(
        "otpauth://totp/Example:alice@example.com?secret=JBSWY3DPEHPK3PXP&issuer=Example",
        "TOTP with issuer in label"
    );

    test_parse_serialize(
        "otpauth://totp/user@example.com?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ&algorithm=SHA512&period=60",
        "TOTP with SHA512 and custom period"
    );

    test_edge_cases();

    printf("\n=== All Tests Completed ===\n");
    return 0;
}