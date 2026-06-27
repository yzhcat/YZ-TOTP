#ifndef IO_PIPE_H_INCLUDED
#define IO_PIPE_H_INCLUDED

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 执行外部程序，通过管道传递数据
 * @param cmd         可执行文件路径（如 "python"）
 * @param argv        参数列表（不含程序名），以 NULL 结尾，可以为 NULL
 * @param stdin_data  输入数据
 * @param stdin_len   输入长度
 * @param stdout_data 输出数据（需 free）
 * @param stdout_len  输出长度
 * @return 0 成功，负值错误码
 */
int io_pipe_exec(const char* cmd, char* const argv[],
                 const void* stdin_data, size_t stdin_len,
                 void** stdout_data, size_t* stdout_len);

#define IO_PIPE_SUCCESS         0
#define IO_PIPE_ERR_PIPE       -1
#define IO_PIPE_ERR_FORK       -2
#define IO_PIPE_ERR_EXEC       -3
#define IO_PIPE_ERR_READ       -4
#define IO_PIPE_ERR_MEMORY     -5
#define IO_PIPE_ERR_CHILD_FAIL -6
#define IO_PIPE_ERR_INVALID    -7

#ifdef __cplusplus
}
#endif

#ifdef IO_PIPE_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(_WIN32) || defined(_WIN64)
#  define IO_PIPE_WINDOWS
#  include <windows.h>
#else
#  define IO_PIPE_POSIX
#  include <unistd.h>
#  include <sys/wait.h>
#endif

static int io_pipe_read_all(void* handle, void** out, size_t* out_len) {
    size_t capacity = 1024;
    size_t total = 0;
    unsigned char* buffer = (unsigned char*)malloc(capacity);
    if (!buffer) return IO_PIPE_ERR_MEMORY;

#ifdef IO_PIPE_WINDOWS
    HANDLE h = (HANDLE)handle;
    DWORD bytes_read;
    while (1) {
        if (total + 4096 > capacity) {
            capacity *= 2;
            unsigned char* newbuf = (unsigned char*)realloc(buffer, capacity);
            if (!newbuf) { free(buffer); return IO_PIPE_ERR_MEMORY; }
            buffer = newbuf;
        }
        if (!ReadFile(h, buffer + total, (DWORD)(capacity - total), &bytes_read, NULL)) {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE) break;
            free(buffer);
            return IO_PIPE_ERR_READ;
        }
        if (bytes_read == 0) break;
        total += bytes_read;
    }
#else
    int fd = (int)(intptr_t)handle;
    ssize_t bytes_read;
    while (1) {
        if (total + 4096 > capacity) {
            capacity *= 2;
            unsigned char* newbuf = (unsigned char*)realloc(buffer, capacity);
            if (!newbuf) { free(buffer); return IO_PIPE_ERR_MEMORY; }
            buffer = newbuf;
        }
        bytes_read = read(fd, buffer + total, capacity - total);
        if (bytes_read < 0) {
            if (errno == EINTR) continue;
            free(buffer);
            return IO_PIPE_ERR_READ;
        }
        if (bytes_read == 0) break;
        total += bytes_read;
    }
#endif

    *out = buffer;
    *out_len = total;
    return IO_PIPE_SUCCESS;
}

int io_pipe_exec(const char* cmd, char* const argv[],
                 const void* stdin_data, size_t stdin_len,
                 void** stdout_data, size_t* stdout_len) {
    if (!cmd || !stdout_data || !stdout_len) {
        return IO_PIPE_ERR_INVALID;
    }

    int ret = IO_PIPE_SUCCESS;

#ifdef IO_PIPE_WINDOWS
    /* Windows 实现：通过 cmd.exe /c 执行 */
    HANDLE h_stdin_rd = NULL, h_stdin_wr = NULL;
    HANDLE h_stdout_rd = NULL, h_stdout_wr = NULL;
    HANDLE h_stderr_rd = NULL, h_stderr_wr = NULL;
    PROCESS_INFORMATION pi = {0};
    STARTUPINFO si = { sizeof(STARTUPINFO) };
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    char* cmdline = NULL;
    DWORD written;
    DWORD exit_code;

    if (!CreatePipe(&h_stdin_rd, &h_stdin_wr, &sa, 0)) goto cleanup;
    if (!CreatePipe(&h_stdout_rd, &h_stdout_wr, &sa, 0)) goto cleanup;
    if (!CreatePipe(&h_stderr_rd, &h_stderr_wr, &sa, 0)) goto cleanup;

    SetHandleInformation(h_stdin_wr, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(h_stdout_rd, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(h_stderr_rd, HANDLE_FLAG_INHERIT, 0);

    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = h_stdin_rd;
    si.hStdOutput = h_stdout_wr;
    si.hStdError = h_stderr_wr;

    // 构建程序名+参数的字符串（cmd 为程序名，argv 为额外参数，不含程序名）
    size_t cmd_len = strlen(cmd) + 1; // 程序名 + 空格
    if (argv) {
        for (char* const* arg = argv; *arg; ++arg) {
            cmd_len += strlen(*arg) + 1; // 参数 + 空格
        }
    }
    char* full_cmd = (char*)malloc(cmd_len + 1);
    if (!full_cmd) { ret = IO_PIPE_ERR_MEMORY; goto cleanup; }
    char* ptr = full_cmd;
    ptr += sprintf(ptr, "%s", cmd);
    if (argv) {
        for (char* const* arg = argv; *arg; ++arg) {
            // 如果参数包含空格或特殊字符，用双引号包裹
            if (strchr(*arg, ' ') != NULL || strchr(*arg, '\t') != NULL) {
                ptr += sprintf(ptr, " \"%s\"", *arg);
            } else {
                ptr += sprintf(ptr, " %s", *arg);
            }
        }
    }

    // 使用 cmd.exe /c 执行完整命令
    const char* cmd_exe = "cmd.exe";
    size_t cmdline_len = strlen(cmd_exe) + 3 + strlen(full_cmd) + 1;
    cmdline = (char*)malloc(cmdline_len);
    if (!cmdline) { free(full_cmd); ret = IO_PIPE_ERR_MEMORY; goto cleanup; }
    sprintf(cmdline, "%s /c %s", cmd_exe, full_cmd);
    free(full_cmd);

    if (!CreateProcess(NULL, cmdline, NULL, NULL, TRUE,
                       CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        ret = IO_PIPE_ERR_FORK;
        goto cleanup;
    }

    CloseHandle(h_stdin_rd); h_stdin_rd = NULL;
    CloseHandle(h_stdout_wr); h_stdout_wr = NULL;
    CloseHandle(h_stderr_wr); h_stderr_wr = NULL;

    if (!WriteFile(h_stdin_wr, stdin_data, (DWORD)stdin_len, &written, NULL) ||
        written != stdin_len) {
        ret = IO_PIPE_ERR_PIPE;
        goto cleanup;
    }
    CloseHandle(h_stdin_wr); h_stdin_wr = NULL;

    ret = io_pipe_read_all((void*)h_stdout_rd, stdout_data, stdout_len);
    if (ret != IO_PIPE_SUCCESS) goto cleanup;

    // 读取 stderr
    void* err_buf = NULL;
    size_t err_len = 0;
    int err_ret = io_pipe_read_all((void*)h_stderr_rd, &err_buf, &err_len);
    if (err_ret == IO_PIPE_SUCCESS && err_len > 0) {
        fwrite(err_buf, 1, err_len, stderr);
        free(err_buf);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exit_code);
    if (exit_code != 0) {
        ret = IO_PIPE_ERR_CHILD_FAIL;
    }

cleanup:
    if (h_stdin_rd) CloseHandle(h_stdin_rd);
    if (h_stdin_wr) CloseHandle(h_stdin_wr);
    if (h_stdout_rd) CloseHandle(h_stdout_rd);
    if (h_stdout_wr) CloseHandle(h_stdout_wr);
    if (h_stderr_rd) CloseHandle(h_stderr_rd);
    if (h_stderr_wr) CloseHandle(h_stderr_wr);
    if (pi.hProcess) CloseHandle(pi.hProcess);
    if (pi.hThread) CloseHandle(pi.hThread);
    free(cmdline);

    if (ret != IO_PIPE_SUCCESS && stdout_data && *stdout_data) {
        free(*stdout_data);
        *stdout_data = NULL;
        *stdout_len = 0;
    }
    return ret;

#else /* IO_PIPE_POSIX */
    /* POSIX 实现：使用 fork+execvp */
    int pipe_in[2], pipe_out[2];
    if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1) {
        return IO_PIPE_ERR_PIPE;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipe_in[0]); close(pipe_in[1]);
        close(pipe_out[0]); close(pipe_out[1]);
        return IO_PIPE_ERR_FORK;
    }

    if (pid == 0) {
        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);
        close(pipe_in[0]); close(pipe_in[1]);
        close(pipe_out[0]); close(pipe_out[1]);

        // 构建 argv 数组：第一个元素为 cmd，后面跟用户提供的参数
        int argc = 0;
        if (argv) {
            while (argv[argc]) argc++;
        }
        char** exec_argv = (char**)malloc((argc + 2) * sizeof(char*));
        if (!exec_argv) {
            fprintf(stderr, "io_pipe: malloc failed\n");
            exit(127);
        }
        exec_argv[0] = (char*)cmd;
        for (int i = 0; i < argc; i++) {
            exec_argv[i+1] = argv[i];
        }
        exec_argv[argc+1] = NULL;

        execvp(cmd, exec_argv);
        free(exec_argv);
        fprintf(stderr, "io_pipe: execvp '%s' failed: %s\n", cmd, strerror(errno));
        exit(127);
    }

    close(pipe_in[0]);
    close(pipe_out[1]);

    ssize_t written = write(pipe_in[1], stdin_data, stdin_len);
    if (written != (ssize_t)stdin_len) {
        ret = IO_PIPE_ERR_PIPE;
        goto cleanup_posix;
    }
    close(pipe_in[1]);

    ret = io_pipe_read_all((void*)(intptr_t)pipe_out[0], stdout_data, stdout_len);
    if (ret != IO_PIPE_SUCCESS) goto cleanup_posix;

    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        ret = IO_PIPE_ERR_CHILD_FAIL;
    }

cleanup_posix:
    close(pipe_out[0]);
    close(pipe_in[1]);
    if (ret != IO_PIPE_SUCCESS && stdout_data && *stdout_data) {
        free(*stdout_data);
        *stdout_data = NULL;
        *stdout_len = 0;
    }
    return ret;
#endif
}

#endif /* IO_PIPE_IMPLEMENTATION */
#endif /* IO_PIPE_H_INCLUDED */