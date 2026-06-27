// gcc -o test_io_pipe.exe test_io_pipe.c
// ./test_io_pipe.exe
#define IO_PIPE_IMPLEMENTATION
#include "../io_pipe.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

void test_with_pipe(const char* desc, const char* cmd, char* const args[], const void* input, size_t len) {
    void* out = NULL;
    size_t out_len = 0;
    printf("\n--- %s ---\n", desc);
    int ret = io_pipe_exec(cmd, args, input, len, &out, &out_len);
    if (ret == 0) {
        printf("success: %.*s (len %zu)\n", (int)out_len, (char*)out, out_len);
        free(out);
    } else {
        printf("error: %d\n", ret);
    }
}

int main() {
#ifdef _WIN32
    char cwd[256];
    GetCurrentDirectory(sizeof(cwd), cwd);
    printf("current working directory: %s\n", cwd);
#else
    char cwd[256];
    getcwd(cwd, sizeof(cwd));
    printf("current working directory: %s\n", cwd);
#endif

    const char* hex_input = "48656c6c6f20544f5450";
    size_t len = strlen(hex_input);

    // 测试1: 内联代码，参数为 -c 和代码
    char* const inline_args[] = {
        "-c",
        "import sys; data=sys.stdin.read().strip(); print(bytes.fromhex(data).decode('utf-8'), end='')",
        NULL
    };
    test_with_pipe("inline code", "python3", inline_args, hex_input, len);

    // 测试2: 外部脚本（相对路径）
    char* const script_rel_args[] = { "decrypt.py", NULL };
    test_with_pipe("external script (relative path)", "python3", script_rel_args, hex_input, len);

    // 测试3: 外部脚本（绝对路径） - 注意使用正斜杠或双反斜杠
    char* const script_abs_args[] = { "D:\\Library\\Documents\\code_ws\\cpp\\YZ-TOTP\\include\\test\\decrypt.py", NULL };
    test_with_pipe("external script (absolute path)", "python3", script_abs_args, hex_input, len);

    // 测试4: system 命令
    printf("\n--- system command ---\n");
    char sys_cmd[512];
    snprintf(sys_cmd, sizeof(sys_cmd),
             "echo %s | python3 -c \"import sys; data=sys.stdin.read().strip(); print(bytes.fromhex(data).decode('utf-8'), end='')\"",
             hex_input);
    int sys_ret = system(sys_cmd);
    printf("system return: %d\n", sys_ret);

    return 0;
}