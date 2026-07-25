#pragma once

#include "launcher_layout.hpp"

#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <string_view>

namespace arpg::launcher {

enum class LauncherButton {
    none,
    start,
    verify,
    save,
    exit,
};

struct LauncherView {
    LauncherLayout layout;
    std::wstring_view status_text;
    std::wstring_view path_text;
    std::wstring_view full_path;
    LauncherButton hovered_button;
    LauncherButton pressed_button;
    LauncherButton focused_button;
    bool start_enabled;
    bool status_ready;
    bool show_full_path;
};

class LauncherRenderer final {
public:
    LauncherRenderer() = default;
    LauncherRenderer(const LauncherRenderer&) = delete;
    LauncherRenderer& operator=(const LauncherRenderer&) = delete;

    bool initialize(HWND window) noexcept;
    void resize(UINT width, UINT height) noexcept;
    void set_dpi(UINT dpi) noexcept;
    HRESULT render(const LauncherView& view) noexcept;
    void discard_device_resources() noexcept;

private:
    HRESULT create_text_formats() noexcept;
    HRESULT ensure_render_target() noexcept;
    HRESULT draw_text(
        std::wstring_view text,
        IDWriteTextFormat* format,
        RectF rectangle,
        ID2D1Brush* brush,
        DWRITE_WORD_WRAPPING wrapping = DWRITE_WORD_WRAPPING_WRAP,
        bool ellipsis = false) noexcept;
    void draw_button(
        RectF rectangle,
        std::wstring_view label,
        LauncherButton button,
        const LauncherView& view) noexcept;

    HWND window_{};
    UINT dpi_{96U};
    Microsoft::WRL::ComPtr<ID2D1Factory> d2d_factory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwrite_factory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> render_target_;
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> background_brush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> white_brush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> green_brush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> red_brush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> silver_brush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> cyan_brush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> button_brush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> pressed_brush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> disabled_brush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> card_brush_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> title_format_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> subtitle_format_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> status_format_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> button_format_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> path_format_;
    Microsoft::WRL::ComPtr<IDWriteInlineObject> ellipsis_;
};

}  // namespace arpg::launcher
