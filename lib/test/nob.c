// totp/nob.c - Build script for test_totp with incremental build
#define NOB_IMPLEMENTATION
#include "nob.h"
#if defined(_WIN32) || defined(_WIN64)
#define OUTPUT_PATH "test_totp.exe"
#else
#define OUTPUT_PATH "./test_totp.bin"
#endif

// Check if any source file is newer than the target
static int needs_rebuild(void) {
    const char *input_paths[] = {
        "../hash-library/sha1.cpp",
        "../hash-library/sha256.cpp",
        "../hash-library/sha512.cpp",
        "test_totp.cpp",
    };
    return nob_needs_rebuild(OUTPUT_PATH, input_paths, sizeof(input_paths)/sizeof(input_paths[0]));
}

int main(int argc, char **argv) {
    // Enable self-rebuilding
    NOB_GO_REBUILD_URSELF(argc, argv);

    Nob_Cmd cmd = {0};
    
    // Check if rebuild is needed
    if (needs_rebuild()) {
        // Compile test_totp with all dependencies
        nob_cmd_append(&cmd, "g++", 
                       "-I.", 
                       "-I../hash-library", 
                       "-I../base32",
                       "-std=c++17",
                       "../hash-library/sha1.cpp",
                       "../hash-library/sha256.cpp",
                       "../hash-library/sha512.cpp",
                       "test_totp.cpp",
                       "-o", OUTPUT_PATH);
        
        if (!nob_cmd_run(&cmd)) return 1;
    } else {
        nob_log(NOB_INFO, "%s is up-to-date, skipping compilation", OUTPUT_PATH);
    }
    
    // Run the test
    nob_cmd_append(&cmd, OUTPUT_PATH);
    if (!nob_cmd_run(&cmd)) return 1;
    
    return 0;
}