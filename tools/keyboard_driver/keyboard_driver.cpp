#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cwctype>
#include <cwchar>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

struct Action {
    WORD virtual_key{};
    DWORD hold_milliseconds{};
};

std::vector<WORD> g_held_keys;

std::wstring to_lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](wchar_t character) {
            return static_cast<wchar_t>(std::towlower(character));
        });
    return value;
}

std::optional<WORD> parse_key(std::wstring_view key) {
    if (key.size() != 1) return std::nullopt;
    const wchar_t character = static_cast<wchar_t>(std::towupper(key.front()));
    switch (character) {
    case L'1': case L'2': case L'3': case L'4': case L'5':
    case L'W': case L'A': case L'S': case L'D':
    case L'J': case L'K': case L'L': case L'E':
    case L'R': return static_cast<WORD>(character);
    default: return std::nullopt;
    }
}

void send_key(WORD virtual_key, bool pressed) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = virtual_key;
    input.ki.dwFlags = pressed ? 0 : KEYEVENTF_KEYUP;
    if (SendInput(1, &input, sizeof(input)) != 1) {
        std::wcerr << L"SendInput failed: " << GetLastError() << L'\n';
    }
}

void release_all_keys() {
    for (const WORD key : g_held_keys) send_key(key, false);
    g_held_keys.clear();
}

BOOL WINAPI console_control_handler(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT
        || event == CTRL_CLOSE_EVENT || event == CTRL_SHUTDOWN_EVENT) {
        release_all_keys();
    }
    return FALSE;
}

DWORD find_process_id(const std::wstring& executable_name) {
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    const std::wstring wanted = to_lower(executable_name);
    DWORD result{};
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (to_lower(entry.szExeFile) == wanted) {
                result = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return result;
}

struct WindowSearch {
    DWORD process_id{};
    HWND result{};
};

BOOL CALLBACK find_window_callback(HWND window, LPARAM parameter) {
    auto& search = *reinterpret_cast<WindowSearch*>(parameter);
    DWORD process_id{};
    GetWindowThreadProcessId(window, &process_id);
    if (process_id == search.process_id && IsWindowVisible(window)
        && GetWindow(window, GW_OWNER) == nullptr) {
        search.result = window;
        return FALSE;
    }
    return TRUE;
}

HWND find_game_window(const std::wstring& executable_name) {
    const DWORD process_id = find_process_id(executable_name);
    if (process_id == 0) return nullptr;
    WindowSearch search{process_id, nullptr};
    EnumWindows(find_window_callback, reinterpret_cast<LPARAM>(&search));
    return search.result;
}

bool activate_game(HWND window) {
    if (window == nullptr) return false;
    ShowWindow(window, SW_RESTORE);
    SetForegroundWindow(window);
    BringWindowToTop(window);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    return GetForegroundWindow() == window;
}

std::optional<Action> parse_sequence_item(const std::wstring& item) {
    const std::size_t separator = item.find(L':');
    if (separator == std::wstring::npos) return std::nullopt;
    const auto key = parse_key(std::wstring_view(item).substr(0, separator));
    if (!key.has_value()) return std::nullopt;
    try {
        const unsigned long hold = std::stoul(item.substr(separator + 1));
        if (hold == 0 || hold > 10000) return std::nullopt;
        return Action{*key, static_cast<DWORD>(hold)};
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::vector<Action>> parse_sequence(const std::wstring& text) {
    std::vector<Action> actions;
    std::size_t start{};
    while (start < text.size()) {
        const std::size_t end = text.find(L',', start);
        const std::wstring item = text.substr(start, end - start);
        const auto action = parse_sequence_item(item);
        if (!action.has_value()) return std::nullopt;
        actions.push_back(*action);
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return actions.empty() ? std::nullopt
                           : std::optional<std::vector<Action>>(actions);
}

std::optional<std::vector<Action>> load_autorun_actions() {
    wchar_t executable_path[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, executable_path, MAX_PATH);
    if (length == 0 || length == MAX_PATH) return std::nullopt;
    std::wstring plan_path(executable_path, length);
    const std::size_t separator = plan_path.find_last_of(L"\\/");
    if (separator == std::wstring::npos) return std::nullopt;
    plan_path.resize(separator + 1);
    plan_path += L"keyboard_driver_autorun.txt";

    std::wifstream plan(plan_path);
    std::wstring first_line;
    if (!plan || !std::getline(plan, first_line)) return std::nullopt;

    std::size_t repeats = 1U;
    std::wstring sequence = first_line;
    constexpr std::wstring_view repeat_prefix = L"repeat=";
    if (first_line.rfind(repeat_prefix, 0U) == 0U) {
        try {
            repeats = std::stoul(first_line.substr(repeat_prefix.size()));
        } catch (...) {
            return std::nullopt;
        }
        if (repeats == 0U || repeats > 10000U
            || !std::getline(plan, sequence)) return std::nullopt;
    }

    const auto one_cycle = parse_sequence(sequence);
    if (!one_cycle.has_value()) return std::nullopt;
    std::vector<Action> actions;
    actions.reserve(one_cycle->size() * repeats);
    for (std::size_t repeat{}; repeat < repeats; ++repeat) {
        actions.insert(actions.end(), one_cycle->begin(), one_cycle->end());
    }
    return actions;
}

void print_usage() {
    std::wcout
        << L"ARPG keyboard driver (external; does not modify the game)\n\n"
        << L"Usage:\n"
        << L"  arpg_keyboard_driver --key d --hold-ms 500\n"
        << L"  arpg_keyboard_driver --sequence \\\"d:600,j:70,l:70,a:600\\\"\n"
        << L"Options:\n"
        << L"  --target <exe>   Target process name (default: arpg_game.exe)\n"
        << L"  --dry-run        Validate and print actions without sending input\n"
        << L"Allowed keys: 1 2 3 4 5 W A S D J K L E R. Holds are 1-10000 ms.\n";
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
    std::wstring target = L"arpg_game.exe";
    std::vector<Action> actions;
    bool dry_run{};

    for (int index = 1; index < argc; ++index) {
        const std::wstring argument = argv[index];
        const auto next = [&]() -> const wchar_t* {
            return index + 1 < argc ? argv[++index] : nullptr;
        };
        if (argument == L"--target") {
            const wchar_t* value = next();
            if (value == nullptr) { print_usage(); return 2; }
            target = value;
        } else if (argument == L"--key") {
            const wchar_t* value = next();
            if (value == nullptr) { print_usage(); return 2; }
            const auto key = parse_key(value);
            if (!key.has_value()) { print_usage(); return 2; }
            if (index + 2 >= argc || std::wstring(argv[++index]) != L"--hold-ms") {
                print_usage(); return 2;
            }
            try {
                const unsigned long hold = std::stoul(argv[++index]);
                if (hold == 0 || hold > 10000) throw std::out_of_range("hold");
                actions.push_back(Action{*key, static_cast<DWORD>(hold)});
            } catch (...) { print_usage(); return 2; }
        } else if (argument == L"--sequence") {
            const wchar_t* value = next();
            if (value == nullptr) { print_usage(); return 2; }
            const auto parsed = parse_sequence(value);
            if (!parsed.has_value()) { print_usage(); return 2; }
            actions = *parsed;
        } else if (argument == L"--dry-run") {
            dry_run = true;
        } else if (argument == L"--help" || argument == L"-h") {
            print_usage(); return 0;
        } else {
            print_usage(); return 2;
        }
    }

    if (actions.empty()) {
        const auto autorun = load_autorun_actions();
        if (!autorun.has_value()) { print_usage(); return 2; }
        actions = *autorun;
        std::wcout << L"Loaded keyboard_driver_autorun.txt\n";
    }
    SetConsoleCtrlHandler(console_control_handler, TRUE);

    std::wcout << L"Target: " << target << L'\n';
    for (const Action& action : actions) {
        std::wcout << L"  " << static_cast<wchar_t>(action.virtual_key)
                   << L" for " << action.hold_milliseconds << L" ms\n";
    }
    if (dry_run) return 0;

    const HWND game_window = find_game_window(target);
    if (!activate_game(game_window)) {
        std::wcerr << L"Could not foreground a visible window for " << target << L".\n";
        return 3;
    }

    for (const Action& action : actions) {
        if (GetForegroundWindow() != game_window) {
            if (!activate_game(game_window)) {
                std::wcerr << L"Could not restore game foreground; stopping safely.\n";
                release_all_keys();
                return 4;
            }
        }
        g_held_keys.push_back(action.virtual_key);
        send_key(action.virtual_key, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(action.hold_milliseconds));
        send_key(action.virtual_key, false);
        g_held_keys.pop_back();
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return 0;
}
