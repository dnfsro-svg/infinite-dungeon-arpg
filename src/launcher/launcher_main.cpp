#include "launcher_core.hpp"
#include "launcher_layout.hpp"
#include "launcher_platform_win32.hpp"
#include "launcher_renderer.hpp"

#include <Windows.h>
#include <windowsx.h>

#include <filesystem>
#include <cmath>
#include <cwchar>
#include <optional>
#include <string>
#include <vector>

namespace arpg::launcher {
namespace {

constexpr wchar_t kWindowClassName[] = L"InfiniteDungeonLauncherWindow";
constexpr wchar_t kWindowTitle[] = L"无限地下城启动器";
constexpr int kLauncherIconId = 101;

bool contains(const RectF rectangle, const float x, const float y) noexcept {
    return x >= rectangle.left && x < rectangle.right &&
        y >= rectangle.top && y < rectangle.bottom;
}

std::optional<std::wstring> local_app_data_value() noexcept {
    try {
        DWORD required = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0U);
        if (required == 0U) {
            return std::nullopt;
        }

        std::vector<wchar_t> buffer(required);
        for (;;) {
            const DWORD length = GetEnvironmentVariableW(
                L"LOCALAPPDATA", buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0U) {
                return std::nullopt;
            }
            if (length < buffer.size()) {
                return std::wstring{buffer.data(), length};
            }
            buffer.resize(static_cast<std::size_t>(length) + 1U);
        }
    }
    catch (...) {
        return std::nullopt;
    }
}

std::wstring render_failure_message(const RendererResult& result) {
    const std::wstring_view stage = renderer_stage_name(result.stage);
    wchar_t hresult[11]{};
    std::swprintf(
        hresult,
        std::size(hresult),
        L"%08X",
        static_cast<unsigned int>(result.hresult));
    return L"渲染失败：" + std::wstring{stage} + L"（HRESULT 0x" + hresult + L"）";
}

RECT fallback_rectangle(const RectF rectangle, const float scale) noexcept {
    return {
        static_cast<LONG>(std::lround(rectangle.left * scale)),
        static_cast<LONG>(std::lround(rectangle.top * scale)),
        static_cast<LONG>(std::lround(rectangle.right * scale)),
        static_cast<LONG>(std::lround(rectangle.bottom * scale)),
    };
}

void draw_fallback_text(
    const HDC device_context,
    const std::wstring_view text,
    const RectF rectangle,
    const float scale,
    const int font_size,
    const COLORREF color,
    const UINT format) noexcept {
    const HFONT font = CreateFontW(
        -static_cast<int>(std::lround(static_cast<float>(font_size) * scale)),
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Microsoft YaHei UI");
    if (font == nullptr) {
        return;
    }
    const HGDIOBJ previous_font = SelectObject(device_context, font);
    SetTextColor(device_context, color);
    SetBkMode(device_context, TRANSPARENT);
    RECT destination = fallback_rectangle(rectangle, scale);
    DrawTextW(
        device_context,
        text.data(),
        static_cast<int>(text.size()),
        &destination,
        format);
    SelectObject(device_context, previous_font);
    DeleteObject(font);
}

void draw_fallback_card(
    const HDC device_context,
    const RectF rectangle,
    const float scale,
    const COLORREF fill_color,
    const COLORREF border_color,
    const int border_width = 1) noexcept {
    const RECT destination = fallback_rectangle(rectangle, scale);
    const HBRUSH brush = CreateSolidBrush(fill_color);
    const HPEN pen = CreatePen(PS_SOLID, border_width, border_color);
    const HGDIOBJ previous_brush = SelectObject(device_context, brush);
    const HGDIOBJ previous_pen = SelectObject(device_context, pen);
    RoundRect(
        device_context,
        destination.left,
        destination.top,
        destination.right,
        destination.bottom,
        static_cast<int>(std::lround(6.0F * scale)),
        static_cast<int>(std::lround(6.0F * scale)));
    SelectObject(device_context, previous_pen);
    SelectObject(device_context, previous_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void draw_fallback_button(
    const HDC device_context,
    const RectF rectangle,
    const std::wstring_view label,
    const LauncherButton button,
    const LauncherView& view,
    const float scale) noexcept {
    const bool disabled = button == LauncherButton::start && !view.start_enabled;
    const bool active = !disabled &&
        (view.hovered_button == button || view.focused_button == button);
    const bool pressed = !disabled && view.pressed_button == button;
    draw_fallback_card(
        device_context,
        rectangle,
        scale,
        disabled ? RGB(85, 91, 99) : (pressed ? RGB(21, 54, 79) : RGB(36, 69, 94)),
        active ? RGB(0, 217, 255) : RGB(192, 199, 209),
        active ? static_cast<int>(std::lround(2.5F * scale))
               : static_cast<int>(std::lround(1.5F * scale)));
    draw_fallback_text(
        device_context,
        label,
        rectangle,
        scale,
        18,
        RGB(255, 255, 255),
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void paint_native_fallback(
    const HDC device_context,
    const HWND window,
    const LauncherView& view,
    const UINT dpi,
    const std::wstring_view message) noexcept {
    const float scale = static_cast<float>(dpi == 0U ? 96U : dpi) / 96.0F;
    RECT client{};
    if (GetClientRect(window, &client) == FALSE) {
        return;
    }
    const HBRUSH background = CreateSolidBrush(RGB(6, 23, 36));
    FillRect(device_context, &client, background);
    DeleteObject(background);

    draw_fallback_card(
        device_context, view.layout.status, scale, RGB(6, 23, 36), RGB(192, 199, 209));
    draw_fallback_button(
        device_context, view.layout.start, L"开始游戏", LauncherButton::start, view, scale);
    draw_fallback_button(
        device_context, view.layout.verify, L"检查游戏文件", LauncherButton::verify, view, scale);
    draw_fallback_button(
        device_context, view.layout.save, L"打开存档目录", LauncherButton::save, view, scale);
    draw_fallback_button(
        device_context, view.layout.exit, L"退出", LauncherButton::exit, view, scale);
    draw_fallback_card(
        device_context, view.layout.path, scale, RGB(6, 23, 36), RGB(192, 199, 209));

    draw_fallback_text(
        device_context,
        L"无限地下城",
        view.layout.title,
        scale,
        32,
        RGB(255, 255, 255),
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_fallback_text(
        device_context,
        L"版本 0.1.0 · 单人无限地下城",
        view.layout.subtitle,
        scale,
        16,
        RGB(255, 255, 255),
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_fallback_text(
        device_context,
        message.empty() ? view.status_text : message,
        view.layout.status,
        scale,
        18,
        message.empty() && view.status_ready ? RGB(0, 255, 0) : RGB(255, 0, 0),
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_fallback_text(
        device_context,
        view.path_text,
        view.layout.path,
        scale,
        13,
        RGB(255, 255, 255),
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (view.show_full_path) {
        const RectF tooltip{58.0F, 430.0F, 862.0F, 522.0F};
        draw_fallback_card(
            device_context, tooltip, scale, RGB(10, 28, 43), RGB(0, 217, 255),
            static_cast<int>(std::lround(2.0F * scale)));
        const RectF tooltip_content{
            tooltip.left + 12.0F,
            tooltip.top + 8.0F,
            tooltip.right - 12.0F,
            tooltip.bottom - 8.0F,
        };
        draw_fallback_text(
            device_context,
            view.full_path,
            tooltip_content,
            scale,
            13,
            RGB(255, 255, 255),
            DT_LEFT | DT_WORDBREAK);
    }
}

class WindowController final {
public:
    void attach(const HWND window) noexcept {
        window_ = window;
        dpi_ = GetDpiForWindow(window_);
        if (dpi_ == 0U) {
            dpi_ = 96U;
        }
    }

    bool create() {
        if (!renderer_.initialize(window_)) {
            return false;
        }
        refresh_installation();
        return true;
    }

    LRESULT handle_message(
        const UINT message,
        const WPARAM w_param,
        const LPARAM l_param) {
        switch (message) {
        case WM_PAINT:
            paint();
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            if (w_param != SIZE_MINIMIZED) {
                const RendererResult result = renderer_.resize(
                    LOWORD(l_param), HIWORD(l_param));
                if (FAILED(result.hresult)) {
                    handle_renderer_failure(result);
                }
            }
            return 0;
        case WM_DPICHANGED:
            handle_dpi_changed(w_param, l_param);
            return 0;
        case WM_MOUSEMOVE:
            handle_mouse_move(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param));
            return 0;
        case WM_MOUSELEAVE:
            tracking_mouse_ = false;
            set_hovered_button(LauncherButton::none);
            set_show_full_path(false);
            return 0;
        case WM_LBUTTONDOWN:
            handle_left_button_down(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param));
            return 0;
        case WM_LBUTTONUP:
            handle_left_button_up(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param));
            return 0;
        case WM_CAPTURECHANGED:
            set_pressed_button(LauncherButton::none);
            return 0;
        case WM_SETFOCUS:
            window_focused_ = true;
            invalidate();
            return 0;
        case WM_KILLFOCUS:
            window_focused_ = false;
            set_pressed_button(LauncherButton::none);
            invalidate();
            return 0;
        case WM_KEYDOWN:
            if (w_param == VK_RETURN) {
                if (status_.ready()) {
                    start_game();
                }
                return 0;
            }
            if (w_param == VK_ESCAPE) {
                DestroyWindow(window_);
                return 0;
            }
            break;
        case WM_DISPLAYCHANGE:
            invalidate();
            return 0;
        case WM_CLOSE:
            DestroyWindow(window_);
            return 0;
        case WM_DESTROY:
            renderer_.discard_device_resources();
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }
        return DefWindowProcW(window_, message, w_param, l_param);
    }

private:
    LauncherLayout layout() const noexcept {
        RECT client{};
        if (GetClientRect(window_, &client) == FALSE || dpi_ == 0U) {
            return {};
        }
        const float scale = static_cast<float>(dpi_) / 96.0F;
        const float width = static_cast<float>(client.right - client.left) / scale;
        const float height = static_cast<float>(client.bottom - client.top) / scale;
        return make_launcher_layout(width, height);
    }

    void refresh_installation() {
        launcher_executable_ = current_executable_path();
        if (!launcher_executable_) {
            status_ = {
                InstallationState::invalid_launcher_path,
                {},
                {},
                {},
            };
            full_path_.clear();
            path_text_ = L"安装目录：不可用";
        }
        else {
            status_ = inspect_installation(*launcher_executable_);
            full_path_ = status_.application_directory.native();
            path_text_ = L"安装目录：" + full_path_;
        }
        status_text_ = installation_message(status_);
        status_ready_ = status_.ready();
    }

    void set_error(const std::wstring& message) {
        status_text_ = message;
        status_ready_ = false;
        invalidate();
    }

    void start_game() {
        if (!status_.ready()) {
            return;
        }

        refresh_installation();
        invalidate();
        if (!status_.ready()) {
            return;
        }

        const std::optional<LaunchRequest> request = make_launch_request(status_);
        if (!request) {
            set_error(L"无法创建游戏启动请求");
            return;
        }
        const PlatformResult result = launch_game(*request);
        if (!result.ok) {
            set_error(result.message);
            return;
        }
        PostQuitMessage(0);
    }

    void verify_installation() {
        refresh_installation();
        invalidate();
    }

    void open_saves() {
        const std::optional<std::wstring> local_app_data = local_app_data_value();
        if (!local_app_data) {
            set_error(L"无法解析 LOCALAPPDATA");
            return;
        }
        const std::optional<std::filesystem::path> save_directory =
            default_save_directory(*local_app_data);
        if (!save_directory) {
            set_error(L"无法解析存档目录");
            return;
        }
        const PlatformResult result = open_save_directory(*save_directory);
        if (!result.ok) {
            set_error(result.message);
        }
    }

    void invoke(const LauncherButton button) {
        switch (button) {
        case LauncherButton::start:
            start_game();
            break;
        case LauncherButton::verify:
            verify_installation();
            break;
        case LauncherButton::save:
            open_saves();
            break;
        case LauncherButton::exit:
            DestroyWindow(window_);
            break;
        case LauncherButton::none:
            break;
        }
    }

    LauncherButton hit_test(const float x, const float y) const noexcept {
        const LauncherLayout current_layout = layout();
        if (status_.ready() && contains(current_layout.start, x, y)) {
            return LauncherButton::start;
        }
        if (contains(current_layout.verify, x, y)) {
            return LauncherButton::verify;
        }
        if (contains(current_layout.save, x, y)) {
            return LauncherButton::save;
        }
        if (contains(current_layout.exit, x, y)) {
            return LauncherButton::exit;
        }
        return LauncherButton::none;
    }

    void logical_pointer(
        const int pixel_x,
        const int pixel_y,
        float& logical_x,
        float& logical_y) const noexcept {
        const float scale = static_cast<float>(dpi_) / 96.0F;
        logical_x = static_cast<float>(pixel_x) / scale;
        logical_y = static_cast<float>(pixel_y) / scale;
    }

    void handle_mouse_move(const int pixel_x, const int pixel_y) {
        if (!tracking_mouse_) {
            TRACKMOUSEEVENT tracking{
                sizeof(tracking), TME_LEAVE, window_, HOVER_DEFAULT};
            if (TrackMouseEvent(&tracking) != FALSE) {
                tracking_mouse_ = true;
            }
        }

        float x = 0.0F;
        float y = 0.0F;
        logical_pointer(pixel_x, pixel_y, x, y);
        set_hovered_button(hit_test(x, y));
        set_show_full_path(contains(layout().path, x, y));
    }

    void handle_left_button_down(const int pixel_x, const int pixel_y) {
        float x = 0.0F;
        float y = 0.0F;
        logical_pointer(pixel_x, pixel_y, x, y);
        const LauncherButton button = hit_test(x, y);
        if (button != LauncherButton::none) {
            SetCapture(window_);
            set_pressed_button(button);
        }
    }

    void handle_left_button_up(const int pixel_x, const int pixel_y) {
        float x = 0.0F;
        float y = 0.0F;
        logical_pointer(pixel_x, pixel_y, x, y);
        const LauncherButton released_over = hit_test(x, y);
        const LauncherButton pressed = pressed_button_;
        if (GetCapture() == window_) {
            ReleaseCapture();
        }
        set_pressed_button(LauncherButton::none);
        set_hovered_button(released_over);
        if (pressed != LauncherButton::none && pressed == released_over) {
            invoke(pressed);
        }
    }

    void set_hovered_button(const LauncherButton button) noexcept {
        if (hovered_button_ != button) {
            hovered_button_ = button;
            invalidate();
        }
    }

    void set_pressed_button(const LauncherButton button) noexcept {
        if (pressed_button_ != button) {
            pressed_button_ = button;
            invalidate();
        }
    }

    void set_show_full_path(const bool show) noexcept {
        if (show_full_path_ != show) {
            show_full_path_ = show;
            invalidate();
        }
    }

    void handle_dpi_changed(const WPARAM w_param, const LPARAM l_param) noexcept {
        dpi_ = HIWORD(w_param);
        if (dpi_ == 0U) {
            dpi_ = 96U;
        }
        renderer_.set_dpi(dpi_);
        const auto* suggested = reinterpret_cast<const RECT*>(l_param);
        SetWindowPos(
            window_,
            nullptr,
            suggested->left,
            suggested->top,
            suggested->right - suggested->left,
            suggested->bottom - suggested->top,
            SWP_NOACTIVATE | SWP_NOZORDER);
        invalidate();
    }

    void paint() noexcept {
        PAINTSTRUCT paint_structure{};
        BeginPaint(window_, &paint_structure);
        const LauncherLayout current_layout = layout();
        const LauncherButton focused_button = window_focused_
            ? (status_.ready() ? LauncherButton::start : LauncherButton::verify)
            : LauncherButton::none;
        const LauncherView view{
            current_layout,
            status_text_,
            path_text_,
            full_path_,
            hovered_button_,
            pressed_button_,
            focused_button,
            status_.ready(),
            status_ready_,
            show_full_path_,
        };
        const RendererResult result = renderer_.render(view);
        if (FAILED(result.hresult)) {
            const std::wstring message = render_failure_message(result);
            handle_renderer_failure(result);
            paint_native_fallback(
                paint_structure.hdc,
                window_,
                view,
                dpi_,
                message);
        }
        else {
            SetWindowTextW(window_, kWindowTitle);
            render_retry_requested_ = false;
            if ((renderer_.window_state() & D2D1_WINDOW_STATE_OCCLUDED) != 0U) {
                paint_native_fallback(
                    paint_structure.hdc,
                    window_,
                    view,
                    dpi_,
                    {});
            }
        }
        EndPaint(window_, &paint_structure);
    }

    void invalidate() noexcept {
        if (window_ != nullptr) {
            InvalidateRect(window_, nullptr, FALSE);
        }
    }

    void handle_renderer_failure(const RendererResult& result) noexcept {
        const std::wstring message = render_failure_message(result);
        SetWindowTextW(window_, (std::wstring{kWindowTitle} + L" - " + message).c_str());
        renderer_.discard_device_resources();
        if (!render_retry_requested_) {
            render_retry_requested_ = true;
            invalidate();
        }
    }

    HWND window_{};
    UINT dpi_{96U};
    LauncherRenderer renderer_;
    std::optional<std::filesystem::path> launcher_executable_;
    InstallationStatus status_{
        InstallationState::invalid_launcher_path, {}, {}, {}};
    std::wstring status_text_;
    std::wstring path_text_;
    std::wstring full_path_;
    LauncherButton hovered_button_{LauncherButton::none};
    LauncherButton pressed_button_{LauncherButton::none};
    bool tracking_mouse_{false};
    bool window_focused_{false};
    bool status_ready_{false};
    bool show_full_path_{false};
    bool render_retry_requested_{false};
};

LRESULT CALLBACK launcher_window_proc(
    const HWND window,
    const UINT message,
    const WPARAM w_param,
    const LPARAM l_param) noexcept {
    try {
        WindowController* controller = reinterpret_cast<WindowController*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(l_param);
            controller = static_cast<WindowController*>(create->lpCreateParams);
            controller->attach(window);
            SetWindowLongPtrW(
                window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(controller));
        }
        if (controller == nullptr) {
            return DefWindowProcW(window, message, w_param, l_param);
        }
        if (message == WM_CREATE) {
            return controller->create() ? 0 : -1;
        }
        return controller->handle_message(message, w_param, l_param);
    }
    catch (...) {
        PostQuitMessage(1);
        return 0;
    }
}

class ComApartment final {
public:
    ComApartment() noexcept : result_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}

    ~ComApartment() {
        if (SUCCEEDED(result_)) {
            CoUninitialize();
        }
    }

    bool available() const noexcept {
        return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE;
    }

private:
    HRESULT result_;
};

int run_launcher(const HINSTANCE instance, const int show_command) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    ComApartment apartment;
    if (!apartment.available()) {
        return 1;
    }

    const HICON icon = static_cast<HICON>(LoadImageW(
        instance,
        MAKEINTRESOURCEW(kLauncherIconId),
        IMAGE_ICON,
        0,
        0,
        LR_DEFAULTSIZE));
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = launcher_window_proc;
    window_class.hInstance = instance;
    window_class.hIcon = icon;
    window_class.hIconSm = icon;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = kWindowClassName;
    if (RegisterClassExW(&window_class) == 0U) {
        return 1;
    }

    const DWORD window_style =
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    const DWORD extended_style = WS_EX_APPWINDOW;
    UINT dpi = GetDpiForSystem();
    if (dpi == 0U) {
        dpi = 96U;
    }
    const PixelSize client_size = launcher_pixel_size(dpi);
    RECT window_rectangle{0, 0, client_size.width, client_size.height};
    if (AdjustWindowRectExForDpi(
            &window_rectangle,
            window_style,
            FALSE,
            extended_style,
            dpi) == FALSE) {
        return 1;
    }

    WindowController controller;
    const HWND window = CreateWindowExW(
        extended_style,
        kWindowClassName,
        kWindowTitle,
        window_style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        window_rectangle.right - window_rectangle.left,
        window_rectangle.bottom - window_rectangle.top,
        nullptr,
        nullptr,
        instance,
        &controller);
    if (window == nullptr) {
        return 1;
    }

    ShowWindow(window, show_command);
    UpdateWindow(window);

    MSG message{};
    for (;;) {
        const BOOL result = GetMessageW(&message, nullptr, 0U, 0U);
        if (result == 0) {
            return static_cast<int>(message.wParam);
        }
        if (result == -1) {
            return 1;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

}  // namespace
}  // namespace arpg::launcher

int WINAPI wWinMain(
    const HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    const int show_command) {
    try {
        return arpg::launcher::run_launcher(instance, show_command);
    }
    catch (...) {
        return 1;
    }
}
