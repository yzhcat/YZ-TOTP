#define NOB_IMPLEMENTATION
#define NOB_UNSTRIP_PREFIX
#include "nob.h"

#include <stdio.h>
#include <sys/stat.h>

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    mkdir("build");
    mkdir("out");

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "cl.exe",
        "/std:c++17",
        "/EHsc",
        "/W4",
        "/nologo",
        "/utf-8",
        "/D_CRT_SECURE_NO_WARNINGS",
        "/I3rdparty/core-ui/include",
        "/Ilib",
        "/Fo:build/",
        "/Fe:out/totp_viewer.exe",
        "main.cpp",
        "lib/hash-library/sha1.cpp",
        "lib/hash-library/sha256.cpp",
        "lib/hash-library/sha512.cpp",
        "3rdparty/core-ui/lib/dynamic/core-ui.lib",
        "/link",
        "/SUBSYSTEM:windows",
        "/MACHINE:X64",
        "user32.lib",
        "gdi32.lib",
        "winmm.lib",
        "comdlg32.lib",
        "shell32.lib",
        "ole32.lib",
        "uuid.lib");

    if (!nob_cmd_run(&cmd)) {
        fprintf(stderr, "Build failed\n");
        return 1;
    }

    // check core-ui.dll exists
    if (!nob_file_exists("out/core-ui.dll")) {
        fprintf(stderr, "out/core-ui.dll not found, copy core-ui.dll to out directory first\n");

        // Copy core-ui.dll to output directory
        if (!nob_copy_file("3rdparty/core-ui/lib/dynamic/core-ui.dll", "out/core-ui.dll")) {
            fprintf(stderr, "Failed to copy core-ui.dll\n");
            return 1;
        }
    }


    return 0;
}