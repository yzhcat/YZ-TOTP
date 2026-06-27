/*
 * TOTP Viewer - Windows GUI application using Core UI
 *
 * Build:
 *  In Visual Studio 2026 Build Tools, run:
 *   ./nob.exe
 *
 * Or manually:
 *   cl.exe /std:c++17 /EHsc /I3rdparty\core-ui\include /Ilib src\main.cpp ^
 *       lib\hash-library\sha1.cpp lib\hash-library\sha256.cpp lib\hash-library\sha512.cpp ^
 *       3rdparty\core-ui\lib\dynamic\core-ui.lib ^
 *       /link /SUBSYSTEM:windows user32.lib gdi32.lib winmm.lib comdlg32.lib shell32.lib
 */

#define TOTP_IMPLEMENTATION
#define BASE32_IMPLEMENTATION
#define OTPAUTH_IMPLEMENTATION
#define IO_PIPE_IMPLEMENTATION

#include "ui_core.h"
#include "otpauth.h"
#include "totp.hpp"
#include "io_pipe.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>
#include <string>
#include <windows.h>

// Default otpauth.txt content
static const char* DEFAULT_OTPAUTH_CONTENT =
    "# TOTP Configuration\n"
    "# Format: otpauth://totp/Name?secret=KEY&algorithm=SHA1&digits=6&period=30\n"
    "# Reference: https://github.com/google/google-authenticator/wiki/Key-Uri-Format\n"
    "\n"
    "otpauth://totp/Example:user@example.com?secret=JBSWY3DPEHPK3PXP&algorithm=SHA1&digits=6&period=30\n"
    "\n"
    "# Add your TOTP entries above, one per line\n";

// ============================================================================
// Configuration
// ============================================================================

struct DecryptConfig {
    std::string program;
    std::string encrypted_file;
};

static DecryptConfig g_decrypt_config;
static std::string g_otpauth_from_ini;  // otpauth path from INI config

// ============================================================================
// Path resolution
// ============================================================================

static std::string get_exe_directory() {
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(NULL, path, MAX_PATH) == 0) {
        return "";
    }
    std::wstring wpath(path);
    size_t pos = wpath.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        // Convert wstring to string
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), (int)pos, NULL, 0, NULL, NULL);
        std::string result(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), (int)pos, &result[0], size_needed, NULL, NULL);
        return result;
    }
    return "";
}

// ============================================================================
// INI parsing
// ============================================================================

static std::string read_ini_string(const char *ini_path, const char *section, const char *key, const char *default_val) {
    char buf[512] = {0};
    GetPrivateProfileStringA(section, key, default_val, buf, sizeof(buf), ini_path);
    return std::string(buf);
}

static void load_config_from_ini(const char *ini_path) {
    FILE *f = fopen(ini_path, "r");
    if (!f) return;
    fclose(f);

    g_decrypt_config.program = read_ini_string(ini_path, "decrypt", "program", "");
    g_decrypt_config.encrypted_file = read_ini_string(ini_path, "decrypt", "file", "");

    // Read otpauth path from [totp_viewer] section
    std::string otpauth_path = read_ini_string(ini_path, "totp_viewer", "otpauth", "");
    if (!otpauth_path.empty()) {
        g_otpauth_from_ini = otpauth_path;
    }
}

// ============================================================================
// Encrypted data loading
// ============================================================================

static void* load_encrypted_data(const char *encrypted_file, const char *decrypt_program, size_t *out_len) {
    FILE *f = fopen(encrypted_file, "rb");
    if (!f) {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    void *encrypted_data = malloc(file_size);
    if (!encrypted_data) {
        fclose(f);
        return NULL;
    }

    fread(encrypted_data, 1, file_size, f);
    fclose(f);

    char *const args[] = { NULL };
    void *decrypted_data = NULL;
    size_t decrypted_len = 0;

    int ret = io_pipe_exec(decrypt_program, args, encrypted_data, file_size, &decrypted_data, &decrypted_len);
    free(encrypted_data);

    if (ret != IO_PIPE_SUCCESS) {
        return NULL;
    }

    *out_len = decrypted_len;
    return decrypted_data;
}

// ============================================================================
// OTPAuth path resolution (default paths only)
// ============================================================================

static std::string resolve_otpauth_path(LPSTR lpCmdLine) {
    (void)lpCmdLine;

    // 1. Check current working directory
    FILE* f = fopen("otpauth.txt", "r");
    if (f) {
        fclose(f);
        return "otpauth.txt";
    }

    // 2. Check exe directory
    std::string exe_dir = get_exe_directory();
    if (!exe_dir.empty()) {
        std::string exe_path = exe_dir + "/otpauth.txt";
        FILE* fe = fopen(exe_path.c_str(), "r");
        if (fe) {
            fclose(fe);
            return exe_path;
        }
    }

    // 3. Create default file in exe directory
    if (!exe_dir.empty()) {
        std::string exe_path = exe_dir + "/otpauth.txt";
        FILE* fc = fopen(exe_path.c_str(), "w");
        if (fc) {
            fputs(DEFAULT_OTPAUTH_CONTENT, fc);
            fclose(fc);
            MessageBoxW(NULL, L"Created default otpauth.txt in exe directory.\nPlease edit it with your TOTP entries.", L"Info", MB_ICONINFORMATION);
            return exe_path;
        }
    }

    return "";
}

// ============================================================================
// Data structures
// ============================================================================

struct TOTPEntry {
    OTPAuthEntry auth;
    int64_t last_update;
    uint32_t current_code;
    int remaining;
    char code_str[9];
    char display_name[256];

    // UI widgets for this entry
    UiWidget container;
    UiWidget code_label;
    UiWidget remaining_label;
    UiWidget copy_button;
    UiWidget progress_bar;
};

static std::vector<TOTPEntry> g_entries;
static UiWindow g_main_window = UI_INVALID;
static HWND g_hwnd = NULL;
static int g_timer_id = 1;

// ============================================================================
// Clipboard
// ============================================================================

static void copy_to_clipboard(const char *text) {
    if (!OpenClipboard(g_hwnd)) return;
    EmptyClipboard();

    size_t len = strlen(text) + 1;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    if (hMem) {
        memcpy(GlobalLock(hMem), text, len);
        GlobalUnlock(hMem);
        SetClipboardData(CF_TEXT, hMem);
    }
    CloseClipboard();
}

// ============================================================================
// TOTP Calculation
// ============================================================================

static void update_totp_entry(TOTPEntry *entry, int64_t now) {
    int algo = totp_parse_algorithm(entry->auth.algorithm);
    if (algo < 0) algo = TOTP_ALGO_SHA1;

    entry->current_code = totp_from_b32(
        entry->auth.secret_b32,
        (TOTPAlgorithm)algo,
        entry->auth.digits,
        entry->auth.period,
        now
    );

    entry->remaining = totp_remaining(entry->auth.period, now);
    entry->last_update = now;

    totp_code_str(entry->current_code, entry->auth.digits, entry->code_str);

    // Update UI
    wchar_t code_wbuf[32] = {0};
    mbstowcs(code_wbuf, entry->code_str, sizeof(entry->code_str));
    ui_label_set_text(entry->code_label, code_wbuf);

    wchar_t rem_wbuf[32];
    swprintf(rem_wbuf, sizeof(rem_wbuf) / sizeof(wchar_t), L"%ds", entry->remaining);
    ui_label_set_text(entry->remaining_label, rem_wbuf);

    // Progress bar: value is remaining time, max is period
    ui_progress_set_value(entry->progress_bar, (float)entry->remaining);

    // Warning color for code label when remaining <= 5 seconds
    if (entry->remaining <= 5) {
        // Code label: light red
        UiColor light_red = {0.85f, 0.4f, 0.4f, 1.0f};
        ui_label_set_text_color(entry->code_label, light_red);
    } else {
        // Restore default accent color
        ui_label_set_text_color(entry->code_label, ui_theme_accent());
    }
}

static void update_all_entries() {
    int64_t now = time(NULL);
    for (auto &entry : g_entries) {
        update_totp_entry(&entry, now);
    }
}

// ============================================================================
// UI Building
// ============================================================================

static void build_entry_widgets(TOTPEntry *entry, UiWidget parent) {
    // Container: vertical box
    UiWidget vbox = ui_vbox();
    ui_widget_set_gap(vbox, 4.0f);
    ui_widget_set_padding_uniform(vbox, 8.0f);
    ui_widget_set_bg_color(vbox, ui_theme_content_bg());
    ui_widget_add_child(parent, vbox);

    // Display name (issuer:account or account)
    wchar_t name_wbuf[512] = {0};
    const char *display = entry->auth.issuer
        ? entry->display_name
        : entry->auth.account;
    if (entry->auth.issuer && entry->auth.account) {
        snprintf(entry->display_name, sizeof(entry->display_name),
                 "%s:%s", entry->auth.issuer, entry->auth.account);
    } else if (entry->auth.account) {
        strncpy(entry->display_name, entry->auth.account, sizeof(entry->display_name) - 1);
    }
    mbstowcs(name_wbuf, display ? display : "", 511);
    UiWidget name_label = ui_label(name_wbuf);
    ui_label_set_bold(name_label, 1);
    ui_label_set_font_size(name_label, 14.0f);
    ui_widget_add_child(vbox, name_label);

    // Code display
    UiWidget code_row = ui_hbox();
    ui_widget_set_gap(code_row, 8.0f);
    ui_widget_add_child(vbox, code_row);

    entry->code_label = ui_label(L"------");
    ui_label_set_font_size(entry->code_label, 28.0f);
    ui_label_set_bold(entry->code_label, 1);
    UiColor accent = ui_theme_accent();
    ui_label_set_text_color(entry->code_label, accent);
    ui_widget_add_child(code_row, entry->code_label);

    // Spacer to push button to the right
    UiWidget code_spacer = ui_spacer(0);
    ui_widget_set_expand(code_spacer, 1);
    ui_widget_add_child(code_row, code_spacer);

    // Copy button
    entry->copy_button = ui_button(L"Copy");
    ui_button_set_type(entry->copy_button, 0); // default type
    ui_widget_set_width(entry->copy_button, 70.0f);
    ui_widget_add_child(code_row, entry->copy_button);

    // Remaining time row
    UiWidget time_row = ui_hbox();
    ui_widget_set_gap(time_row, 8.0f);
    ui_widget_add_child(vbox, time_row);

    entry->remaining_label = ui_label(L"30s");
    ui_label_set_font_size(entry->remaining_label, 12.0f);
    ui_widget_add_child(time_row, entry->remaining_label);

    // Spacer
    UiWidget spacer = ui_spacer(0);
    ui_widget_set_expand(spacer, 1);
    ui_widget_add_child(time_row, spacer);

    // Progress bar (shows remaining time) - expand to fill remaining space
    entry->progress_bar = ui_progress_bar(0.0f, (float)entry->auth.period, (float)entry->auth.period);
    ui_widget_set_height(entry->progress_bar, 6.0f);
    ui_widget_set_expand(entry->progress_bar, 1);
    ui_widget_add_child(time_row, entry->progress_bar);

    // Separator
    UiWidget sep = ui_separator();
    ui_widget_add_child(vbox, sep);

    entry->container = vbox;
}

// ============================================================================
// Entry Management
// ============================================================================

static void add_entry(OTPAuthEntry *auth) {
    TOTPEntry entry = {0};
    memcpy(&entry.auth, auth, sizeof(OTPAuthEntry));

    g_entries.push_back(entry);
}

static void load_otpauth_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open %s\n", path);
        return;
    }

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        // Remove trailing newline
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0) continue;

        OTPAuthEntry auth;
        otpauth_init(&auth);
        if (otpauth_parse(line, &auth)) {
            add_entry(&auth);
        } else {
            fprintf(stderr, "Failed to parse: %s\n", line);
        }
    }
    fclose(fp);
}

static void load_otpauth_from_memory(const char *data, size_t len) {
    const char *ptr = data;
    const char *end = data + len;

    while (ptr < end) {
        const char *line_end = (const char *)memchr(ptr, '\n', end - ptr);
        if (!line_end) line_end = end;

        size_t line_len = line_end - ptr;
        if (line_len > 0) {
            char *line = (char *)malloc(line_len + 1);
            if (line) {
                memcpy(line, ptr, line_len);
                line[line_len] = '\0';

                size_t actual_len = strlen(line);
                while (actual_len > 0 && (line[actual_len-1] == '\n' || line[actual_len-1] == '\r')) {
                    line[--actual_len] = '\0';
                }

                if (actual_len > 0) {
                    OTPAuthEntry auth;
                    otpauth_init(&auth);
                    if (otpauth_parse(line, &auth)) {
                        add_entry(&auth);
                    }
                }

                free(line);
            }
        }

        ptr = line_end + 1;
    }
}

// ============================================================================
// Window Callbacks
// ============================================================================

static void on_copy_click(UiWidget widget, void *userdata) {
    (void)widget;
    TOTPEntry *entry = (TOTPEntry *)userdata;
    copy_to_clipboard(entry->code_str);

    // Show toast
    ui_toast(g_main_window, L"Code copied!", 1500);
}

static void on_timer() {
    update_all_entries();
}

static void on_close(UiWindow win, void *userdata) {
    (void)win;
    (void)userdata;
    KillTimer(g_hwnd, g_timer_id);
    ui_quit(0);
}

// ============================================================================
// Main
// ============================================================================

static std::string g_otpauth_path;
static std::string g_source_info;

int main_impl();

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)nCmdShow;

    bool show_source = false;

    // Check for /SHOW_SOURCE parameter
    if (lpCmdLine && lpCmdLine[0] != '\0') {
        if (strstr(lpCmdLine, "/SHOW_SOURCE") != NULL) {
            show_source = true;
        }
    }

    // Priority 1: /OTPAUTH command line parameter (highest priority, plain text)
    if (lpCmdLine && lpCmdLine[0] != '\0') {
        const char* param = strstr(lpCmdLine, "/OTPAUTH:");
        if (param) {
            param += 9;
            const char* end = strpbrk(param, " \t");
            std::string path;
            if (end) {
                path = std::string(param, end - param);
            } else {
                path = std::string(param);
            }
            if (!path.empty()) {
                FILE* f = fopen(path.c_str(), "r");
                if (f) {
                    fclose(f);
                    g_otpauth_path = path;
                    g_source_info = "Command line: " + path;
                    if (show_source) {
                        MessageBoxW(NULL, std::wstring(g_source_info.begin(), g_source_info.end()).c_str(), L"Data Source", MB_ICONINFORMATION);
                    }
                    return main_impl();
                } else {
                    std::wstring msg = L"File not found: " + std::wstring(path.begin(), path.end());
                    MessageBoxW(NULL, msg.c_str(), L"Error", MB_ICONERROR);
                    return 1;
                }
            }
        }
    }

    // Priority 2: /INI_PATH command line parameter
    bool has_custom_ini = false;
    if (lpCmdLine && lpCmdLine[0] != '\0') {
        const char* param = strstr(lpCmdLine, "/INI_PATH:");
        if (param) {
            param += 10;
            const char* end = strpbrk(param, " \t");
            std::string custom_ini;
            if (end) {
                custom_ini = std::string(param, end - param);
            } else {
                custom_ini = std::string(param);
            }

            FILE* test = fopen(custom_ini.c_str(), "r");
            if (!test) {
                std::wstring msg = L"INI file not found: " + std::wstring(custom_ini.begin(), custom_ini.end());
                MessageBoxW(NULL, msg.c_str(), L"Error", MB_ICONERROR);
                return 1;
            }
            fclose(test);

            load_config_from_ini(custom_ini.c_str());
            has_custom_ini = true;
        }
    }

    // Priority 3: INI config from exe directory
    if (!has_custom_ini) {
        std::string exe_dir = get_exe_directory();
        std::string ini_path = exe_dir + "/totp_viewer.ini";
        load_config_from_ini(ini_path.c_str());
    }

    // Priority 4: Decrypt config from INI (if configured)
    if (!g_decrypt_config.program.empty() && !g_decrypt_config.encrypted_file.empty()) {
        g_source_info = "INI config: encrypted file via " + g_decrypt_config.program;
        if (show_source) {
            MessageBoxW(NULL, std::wstring(g_source_info.begin(), g_source_info.end()).c_str(), L"Data Source", MB_ICONINFORMATION);
        }
        return main_impl();
    }

    // Priority 5: Plain text otpauth from INI
    if (!g_otpauth_from_ini.empty()) {
        FILE* f = fopen(g_otpauth_from_ini.c_str(), "r");
        if (f) {
            fclose(f);
            g_otpauth_path = g_otpauth_from_ini;
            g_source_info = "INI config: " + g_otpauth_path;
            if (show_source) {
                MessageBoxW(NULL, std::wstring(g_source_info.begin(), g_source_info.end()).c_str(), L"Data Source", MB_ICONINFORMATION);
            }
            return main_impl();
        }
    }

    // Priority 6: Default path resolution
    g_otpauth_path = resolve_otpauth_path(lpCmdLine);
    if (g_otpauth_path.empty()) {
        MessageBoxW(NULL, L"Failed to resolve otpauth.txt path", L"Error", MB_ICONERROR);
        return 1;
    }
    g_source_info = "Default path: " + g_otpauth_path;
    if (show_source) {
        MessageBoxW(NULL, std::wstring(g_source_info.begin(), g_source_info.end()).c_str(), L"Data Source", MB_ICONINFORMATION);
    }

    return main_impl();
}

int wmain(int argc, wchar_t **argv) {
    (void)argc;
    (void)argv;
    return main_impl();
}

int main_impl() {
    // Initialize Core UI (returns 0 on success)
    if (ui_init() != 0) {
        MessageBoxW(NULL, L"Failed to initialize UI", L"Error", MB_ICONERROR);
        return 1;
    }

    // Load otpauth entries
    if (!g_decrypt_config.program.empty() && !g_decrypt_config.encrypted_file.empty()) {
        // Load from encrypted file via decrypt program
        size_t decrypted_len = 0;
        void *decrypted_data = load_encrypted_data(
            g_decrypt_config.encrypted_file.c_str(),
            g_decrypt_config.program.c_str(),
            &decrypted_len);

        if (!decrypted_data) {
            wchar_t msg[512];
            swprintf(msg, sizeof(msg) / sizeof(wchar_t),
                L"Failed to decrypt file:\n%S\nusing program:\n%S",
                g_decrypt_config.encrypted_file.c_str(),
                g_decrypt_config.program.c_str());
            MessageBoxW(NULL, msg, L"Error", MB_ICONERROR);
            return 1;
        }

        load_otpauth_from_memory((const char*)decrypted_data, decrypted_len);
        free(decrypted_data);
    } else {
        // Load from regular otpauth file
        load_otpauth_file(g_otpauth_path.c_str());
    }

    if (g_entries.empty()) {
        wchar_t msg[512];
        if (!g_decrypt_config.program.empty()) {
            swprintf(msg, sizeof(msg) / sizeof(wchar_t), L"No valid otpauth entries found in decrypted data");
        } else {
            swprintf(msg, sizeof(msg) / sizeof(wchar_t), L"No valid otpauth entries found in:\n%S", g_otpauth_path.c_str());
        }
        MessageBoxW(NULL, msg, L"Error", MB_ICONWARNING);
        return 1;
    }

    // Create window
    UiWindowConfig config = {0};
    config.title = L"TOTP Viewer";
    config.width = 380;
    config.height = 500;
    config.resizable = 1;
    config.system_frame = 1; // Use system title bar

    g_main_window = ui_window_create(&config);
    if (!g_main_window) {
        MessageBoxW(NULL, L"Failed to create window", L"Error", MB_ICONERROR);
        return 1;
    }

    g_hwnd = (HWND)ui_window_hwnd(g_main_window);

    // Create scrollable content area
    UiWidget scroll = ui_scroll_view();
    ui_widget_set_expand(scroll, 1);

    // Main container (vbox)
    UiWidget main_vbox = ui_vbox();
    ui_widget_set_gap(main_vbox, 0);
    ui_scroll_set_content(scroll, main_vbox);

    // Build UI for each entry
    for (size_t i = 0; i < g_entries.size(); i++) {
        TOTPEntry *entry = &g_entries[i];

        // Build widgets for this entry
        UiWidget entry_container = ui_vbox();
        ui_widget_set_gap(entry_container, 0);
        ui_widget_set_padding_uniform(entry_container, 0);
        build_entry_widgets(entry, entry_container);

        // Add to main container
        ui_widget_add_child(main_vbox, entry_container);

        // Register copy callback
        ui_widget_on_click(entry->copy_button, on_copy_click, entry);
    }

    // Set window root
    ui_window_set_root(g_main_window, scroll);

    // Initial TOTP update
    update_all_entries();

    // Set up timer for updates (every second)
    SetTimer(g_hwnd, g_timer_id, 1000, NULL);

    // Register close callback
    ui_window_on_close(g_main_window, on_close, NULL);

    // Show window
    ui_window_show(g_main_window);

    // Run message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_TIMER && msg.hwnd == g_hwnd && msg.wParam == g_timer_id) {
            on_timer();
        } else if (msg.message == WM_KEYDOWN && msg.hwnd == g_hwnd) {
            if (msg.wParam == VK_ESCAPE) {
                on_close(g_main_window, NULL);
                continue;
            } else if (msg.wParam == 'W' || msg.wParam == 'w') {
                if (GetKeyState(VK_CONTROL) & 0x8000) {
                    on_close(g_main_window, NULL);
                    continue;
                }
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // Cleanup
    for (auto &entry : g_entries) {
        otpauth_free(&entry.auth);
    }

    ui_window_destroy(g_main_window);
    ui_shutdown();

    return 0;
}