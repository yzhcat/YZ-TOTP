// gcc test_base32.c -o test_base32 && ./test_base32 
#define BASE32_IMPLEMENTATION
#include "../base32.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main() {
    const char *input = "Hello, world!";
    size_t enc_len = base32_encsize(strlen(input));
    char *enc = malloc(enc_len + 1);
    
    size_t out_len = base32_encode(enc, input, strlen(input));
    enc[out_len] = '\0';
    printf("Encoded: %s\n", enc);
    
    size_t dec_len = base32_decsize(out_len);
    char *dec = malloc(dec_len);
    size_t written = base32_decode(dec, dec_len, enc);
    printf("Decoded: %.*s\n", (int)written, dec);

    // his Base32 encoded key "JBSWY3DPEHPK3PXP" has the value:
    // byte[] key = { 'H', 'e', 'l', 'l', 'o', '!', (byte) 0xDE, (byte) 0xAD, (byte) 0xBE, (byte) 0xEF };
    
    const char *test_input = "JBSWY3DPEHPK3PXP";
    size_t test_dec_len = base32_decsize(strlen(test_input));
    char *test_dec = malloc(test_dec_len);
    size_t test_written = base32_decode(test_dec, test_dec_len, test_input);
    printf("Test Decoded: %.*s\n", (int)test_written, test_dec);
    // 修改测试部分打印 hex
    printf("Test Decoded (hex): ");
    for (size_t i = 0; i < test_written; i++)
        printf("%02X ", (unsigned char)test_dec[i]);
    printf("\n");
    printf("Expected (hex): 48 65 6C 6C 6F 21 DE AD BE EF\n");

    free(enc);
    free(dec);
    free(test_dec);
    return 0;
}