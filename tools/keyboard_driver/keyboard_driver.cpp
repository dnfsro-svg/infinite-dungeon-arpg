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
    std::vector<WORD> virtual_keys;
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
    const std::wstring normalized = to_lower(std::wstring(key));
    if (normalized == L"f1") return VK_F1;
    if (normalized == L"f12") return VK_F12;
    if (normalized == L"esc") return VK_ESCAPE;
    if (normalized == L"enter") return VK_RETURN;
    if (normalized == L"up") return VK_UP;
    if (normalized == L"down") return VK_DOWN;
    if (normalized == L"left") return VK_LEFT;
    if (normalized == L"right") return VK_RIGHT;
    if (key.size() != 1) return std::nullopt;
    const wchar_t character = static_cast<wchar_t>(std::towupper(key.front()));
    switch (character) {
    case L'1': return VK_NUMPAD1;
    case L'2': return VK_NUMPAD2;
    case L'3': return VK_NUMPAD3;
    case L'4': return VK_NUMPAD4;
    case L'5': return VK_NUMPAD5;
    case L'W': case L'A': case L'S': case L'D': case L'I': case L'P':
    case L'J': case L'K': case L'L': case L'E':
    case L'R': return static_cast<WORD>(character);
    default: return std::nullopt;
    }
}

const wchar_t* special_key_label(WORD virtual_key) noexcept {
    switch (virtual_key) {
    case VK_NUMPAD1: return L"Num1";
    case VK_NUMPAD2: return L"Num2";
    case VK_NUMPAD3: return L"Num3";
    case VK_NUMPAD4: return L"Num4";
    case VK_NUMPAD5: return L"Num5";
    case VK_F1: return L"F1";
    case VK_F12: return L"F12";
    case VK_ESCAPE: return L"Esc";
    case VK_RETURN: return L"Enter";
    case VK_UP: return L"Up";
    case VK_DOWN: return L"Down";
    case VK_LEFT: return L"Left";
    case VK_RIGHT: return L"Right";
    default: return nullptr;
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
    for (auto key = g_held_keys.rbegin(); key != g_held_keys.rend(); ++key) {
        send_key(*key, false);
    }
    g_held_keys.clear();
}

BOOL WINAPI console_control_handler(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT
        || event == CTRL_CLOSE_EVENT || event == CTRL_SHUTDOWN_EVENT) {
        release_all_keys();
    }
    return FALSE;
}

std::vector<DWORD> find_process_ids(const std::wstring& executable_name) {
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return {};

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    const std::wstring wanted = to_lower(executable_name);
    std::vector<DWORD> results;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (to_lower(entry.szExeFile) == wanted) {
                results.push_back(entry.th32ProcessID);
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return results;
}

struct WindowSearch {
    const std::vector<DWORD>* process_ids{};
    std::vector<std::pair<DWORD, HWND>>* results{};
};

struct TargetWindowSearch {
    bool requested_process_matches{true};
    std::vector<std::pair<DWORD, HWND>> candidates;
};

BOOL CALLBACK find_window_callback(HWND window, LPARAM parameter) {
    auto& search = *reinterpret_cast<WindowSearch*>(parameter);
    DWORD process_id{};
    GetWindowThreadProcessId(window, &process_id);
    if (std::find(search.process_ids->begin(), search.process_ids->end(),
            process_id) != search.process_ids->end()
        && IsWindowVisible(window)
        && GetWindow(window, GW_OWNER) == nullptr) {
        search.results->emplace_back(process_id, window);
    }
    return TRUE;
}

TargetWindowSearch find_game_windows(const std::wstring& executable_name,
    std::optional<DWORD> requested_process_id) {
    std::vector<DWORD> process_ids = find_process_ids(executable_name);
    TargetWindowSearch result;
    if (requested_process_id.has_value()) {
        const auto requested = std::find(process_ids.begin(), process_ids.end(),
            *requested_process_id);
        if (requested == process_ids.end()) {
            result.requested_process_matches = false;
            return result;
        }
        process_ids = {*requested_process_id};
    }
    if (process_ids.empty()) return result;
    WindowSearch search{&process_ids, &result.candidates};
    EnumWindows(find_window_callback, reinterpret_cast<LPARAM>(&search));
    return result;
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
        return Action{{*key}, static_cast<DWORD>(hold)};
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

std::optional<std::vector<WORD>> parse_chord(const std::wstring& text) {
    std::vector<WORD> keys;
    std::size_t start{};
    while (start < text.size()) {
        const std::size_t end = text.find(L'+', start);
        const std::wstring token = text.substr(start, end - start);
        const auto key = parse_key(token);
        if (!key.has_value()) return std::nullopt;
        keys.push_back(*key);
        if (end == std::wstring::npos) break;
        start = end + 1U;
    }
    return keys.size() >= 2U
        ? std::optional<std::vector<WORD>>(keys)
        : std::nullopt;
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
        << L"  arpg_keyboard_driver --chord d+j --hold-ms 500\n"
        << L"  arpg_keyboard_driver --sequence \\\"d:600,j:70,l:70,a:600\\\"\n"
        << L"Options:\n"
        << L"  --target <exe>   Target process name (default: arpg_game.exe)\n"
        << L"  --pid <decimal>  Select one matching process explicitly\n"
        << L"  --chord <a+b>    Press in order, hold once, release in reverse\n"
        << L"  --dry-run        Validate and print actions without sending input\n"
        << L"Allowed keys: 1 2 3 4 5 W A S D I P J K L E R F1 F12 Esc "
        << L"Enter Up Down Left Right. "
        << L"1-5 send Num1-Num5; holds are 1-10000 ms.\n";
}

void print_key_label(WORD virtual_key) {
    const wchar_t* const special_label = special_key_label(virtual_key);
    if (special_label != nullptr) {
        std::wcout << special_label;
    } else {
        std::wcout << static_cast<wchar_t>(virtual_key);
    }
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
    std::wstring target = L"arpg_game.exe";
    std::optional<DWORD> requested_process_id;
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
        } else if (argument == L"--pid") {
            const wchar_t* value = next();
            if (value == nullptr || requested_process_id.has_value()) {
                print_usage(); return 2;
            }
            const std::wstring text = value;
            if (text.empty() || !std::all_of(text.begin(), text.end(),
                    [](wchar_t character) {
                        return character >= L'0' && character <= L'9';
                    })) {
                print_usage(); return 2;
            }
            try {
                const unsigned long long parsed = std::stoull(text);
                if (parsed == 0U || parsed > MAXDWORD) {
                    throw std::out_of_range("pid");
                }
                requested_process_id = static_cast<DWORD>(parsed);
            } catch (...) { print_usage(); return 2; }
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
                actions.push_back(Action{{*key}, static_cast<DWORD>(hold)});
            } catch (...) { print_usage(); return 2; }
        } else if (argument == L"--chord") {
            const wchar_t* value = next();
            if (value == nullptr) { print_usage(); return 2; }
            const auto keys = parse_chord(value);
            if (!keys.has_value()) { print_usage(); return 2; }
            if (index + 2 >= argc || std::wstring(argv[++index]) != L"--hold-ms") {
                print_usage(); return 2;
            }
            try {
                const unsigned long hold = std::stoul(argv[++index]);
                if (hold == 0 || hold > 10000) {
                    throw std::out_of_range("hold");
                }
                actions.push_back(Action{*keys, static_cast<DWORD>(hold)});
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
        if (action.virtual_keys.size() == 1U) {
            std::wcout << L"  ";
            print_key_label(action.virtual_keys.front());
            std::wcout << L" for " << action.hold_milliseconds << L" ms\n";
        } else {
            std::wcout << L"  Chord: press ";
            for (std::size_t index = 0; index < action.virtual_keys.size(); ++index) {
                if (index > 0U) std::wcout << L" + ";
                print_key_label(action.virtual_keys[index]);
            }
            std::wcout << L", hold " << action.hold_milliseconds
                       << L" ms, release ";
            for (std::size_t index = action.virtual_keys.size(); index > 0U; --index) {
                if (index < action.virtual_keys.size()) std::wcout << L" + ";
                print_key_label(action.virtual_keys[index - 1U]);
            }
            std::wcout << L'\n';
        }
    }
    if (dry_run) return 0;

    const TargetWindowSearch search =
        find_game_windows(target, requested_process_id);
    if (!search.requested_process_matches) {
        std::wcerr << L"PID " << *requested_process_id
                   << L" does not match target executable " << target << L".\n";
        return 3;
    }
    const auto& candidates = search.candidates;
    if (candidates.size() > 1U) {
        std::wcerr << L"Multiple visible top-level windows for " << target
                   << L"; refusing to choose. Candidate PIDs:";
        std::vector<DWORD> reported_process_ids;
        for (const auto& [process_id, window] : candidates) {
            (void)window;
            if (std::find(reported_process_ids.begin(),
                    reported_process_ids.end(), process_id)
                == reported_process_ids.end()) {
                reported_process_ids.push_back(process_id);
                std::wcerr << L' ' << process_id;
            }
        }
        std::wcerr << L".\n";
        return 3;
    }
    const HWND game_window = candidates.empty() ? nullptr : candidates.front().second;
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
        for (const WORD key : action.virtual_keys) {
            g_held_keys.push_back(key);
            send_key(key, true);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(action.hold_milliseconds));
        for (auto key = action.virtual_keys.rbegin();
             key != action.virtual_keys.rend(); ++key) {
            send_key(*key, false);
        }
        g_held_keys.clear();
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return 0;
}
