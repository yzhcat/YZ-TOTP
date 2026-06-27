/*
 * TOTP Viewer - Windows GUI application using Core UI
 *
 * Build:
 *   ./build.nob
 *
 * Or manually:
 *   cl.exe /std:c++17 /EHsc /I3rdparty\core-ui\include /Ilib main.cpp ^
 *       lib\hash-library\sha1.cpp lib\hash-library\sha256.cpp lib\hash-library\sha512.cpp ^
 *       3rdparty\core-ui\lib\dynamic\core-ui.lib ^
 *       /link /SUBSYSTEM:windows user32.lib gdi32.lib winmm.lib comdlg32.lib shell32.lib
 */

#define TOTP_IMPLEMENTATION
#define BASE32_IMPLEMENTATION
#define OTPAUTH_IMPLEMENTATION

#include "ui_core.h"
#include "otpauth.h"
#include "totp.hpp"

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

static std::string resolve_otpauth_path(LPSTR lpCmdLine) {
    // 1. Check command line parameter /OTPAUTH:path
    if (lpCmdLine && lpCmdLine[0] != '\0') {
        const char* param = strstr(lpCmdLine, "/OTPAUTH:");
        if (param) {
            param += 9; // Skip "/OTPAUTH:"
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
                    return path;
                }
            }
        }
    }

    // 2. Check current working directory
    FILE* f = fopen("otpauth.txt", "r");
    if (f) {
        fclose(f);
        return "otpauth.txt";
    }

    // 3. Check exe directory
    std::string exe_dir = get_exe_directory();
    if (!exe_dir.empty()) {
        std::string exe_path = exe_dir + "/otpauth.txt";
        FILE* fe = fopen(exe_path.c_str(), "r");
        if (fe) {
            fclose(fe);
            return exe_path;
        }
    }

    // 4. Create default file in exe directory
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

int main_impl();

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)nCmdShow;

    // Resolve otpauth.txt path
    g_otpauth_path = resolve_otpauth_path(lpCmdLine);
    if (g_otpauth_path.empty()) {
        MessageBoxW(NULL, L"Failed to resolve otpauth.txt path", L"Error", MB_ICONERROR);
        return 1;
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
    load_otpauth_file(g_otpauth_path.c_str());

    if (g_entries.empty()) {
        wchar_t msg[512];
        swprintf(msg, sizeof(msg) / sizeof(wchar_t), L"No valid otpauth entries found in:\n%S", g_otpauth_path.c_str());
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