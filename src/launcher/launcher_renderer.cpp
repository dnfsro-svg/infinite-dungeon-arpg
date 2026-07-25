#include "launcher_renderer.hpp"

#include <d2d1helper.h>

#include <array>
#include <limits>

namespace arpg::launcher {
namespace {

constexpr wchar_t kFontFamily[] = L"Microsoft YaHei UI";
constexpr RectF kTooltipRectangle{58.0F, 430.0F, 862.0F, 522.0F};

D2D1_RECT_F to_d2d_rectangle(const RectF rectangle) noexcept {
    return D2D1::RectF(
        rectangle.left, rectangle.top, rectangle.right, rectangle.bottom);
}

D2D1_ROUNDED_RECT rounded_rectangle(const RectF rectangle) noexcept {
    return D2D1::RoundedRect(to_d2d_rectangle(rectangle), 6.0F, 6.0F);
}

}  // namespace

bool LauncherRenderer::initialize(const HWND window) noexcept {
    window_ = window;
    dpi_ = GetDpiForWindow(window_);
    if (dpi_ == 0U) {
        dpi_ = 96U;
    }

    HRESULT result = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        d2d_factory_.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        return false;
    }
    result = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(dwrite_factory_.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
        return false;
    }
    return SUCCEEDED(create_text_formats());
}

HRESULT LauncherRenderer::create_text_formats() noexcept {
    const auto create_format = [this](
                                   const FLOAT size,
                                   const DWRITE_FONT_WEIGHT weight,
                                   IDWriteTextFormat** destination) noexcept {
        return dwrite_factory_->CreateTextFormat(
            kFontFamily,
            nullptr,
            weight,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            size,
            L"zh-CN",
            destination);
    };

    HRESULT result = create_format(
        42.0F, DWRITE_FONT_WEIGHT_BOLD, title_format_.ReleaseAndGetAddressOf());
    if (SUCCEEDED(result)) {
        result = create_format(
            18.0F,
            DWRITE_FONT_WEIGHT_NORMAL,
            subtitle_format_.ReleaseAndGetAddressOf());
    }
    if (SUCCEEDED(result)) {
        result = create_format(
            24.0F,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            status_format_.ReleaseAndGetAddressOf());
    }
    if (SUCCEEDED(result)) {
        result = create_format(
            22.0F,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            button_format_.ReleaseAndGetAddressOf());
    }
    if (SUCCEEDED(result)) {
        result = create_format(
            15.0F,
            DWRITE_FONT_WEIGHT_NORMAL,
            path_format_.ReleaseAndGetAddressOf());
    }
    if (FAILED(result)) {
        return result;
    }

    title_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    title_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    subtitle_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    subtitle_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    status_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    status_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    button_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    button_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    path_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    path_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    return dwrite_factory_->CreateEllipsisTrimmingSign(
        path_format_.Get(), ellipsis_.ReleaseAndGetAddressOf());
}

HRESULT LauncherRenderer::ensure_render_target() noexcept {
    if (render_target_) {
        return S_OK;
    }

    RECT client{};
    if (GetClientRect(window_, &client) == FALSE) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    const D2D1_SIZE_U pixel_size = D2D1::SizeU(
        static_cast<UINT>(client.right - client.left),
        static_cast<UINT>(client.bottom - client.top));
    HRESULT result = d2d_factory_->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(window_, pixel_size),
        render_target_.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        return result;
    }
    render_target_->SetDpi(static_cast<FLOAT>(dpi_), static_cast<FLOAT>(dpi_));

    const std::array<D2D1_GRADIENT_STOP, 2U> stops{{
        {0.0F, D2D1::ColorF(0x061724, 1.0F)},
        {1.0F, D2D1::ColorF(0x36566F, 1.0F)},
    }};
    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> stop_collection;
    result = render_target_->CreateGradientStopCollection(
        stops.data(),
        static_cast<UINT32>(stops.size()),
        stop_collection.GetAddressOf());
    if (SUCCEEDED(result)) {
        const D2D1_SIZE_F size = render_target_->GetSize();
        result = render_target_->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(
                D2D1::Point2F(0.0F, 0.0F), D2D1::Point2F(size.width, size.height)),
            stop_collection.Get(),
            background_brush_.ReleaseAndGetAddressOf());
    }

    const auto create_solid = [this](
                                  const UINT32 color,
                                  ID2D1SolidColorBrush** destination) noexcept {
        return render_target_->CreateSolidColorBrush(
            D2D1::ColorF(color, 1.0F), destination);
    };
    if (SUCCEEDED(result)) {
        result = create_solid(0xFFFFFF, white_brush_.ReleaseAndGetAddressOf());
    }
    if (SUCCEEDED(result)) {
        result = create_solid(0x00FF00, green_brush_.ReleaseAndGetAddressOf());
    }
    if (SUCCEEDED(result)) {
        result = create_solid(0xFF0000, red_brush_.ReleaseAndGetAddressOf());
    }
    if (SUCCEEDED(result)) {
        result = create_solid(0xC0C7D1, silver_brush_.ReleaseAndGetAddressOf());
    }
    if (SUCCEEDED(result)) {
        result = create_solid(0x00D9FF, cyan_brush_.ReleaseAndGetAddressOf());
    }
    if (SUCCEEDED(result)) {
        result = create_solid(0x24455E, button_brush_.ReleaseAndGetAddressOf());
    }
    if (SUCCEEDED(result)) {
        result = create_solid(0x15364F, pressed_brush_.ReleaseAndGetAddressOf());
    }
    if (SUCCEEDED(result)) {
        result = create_solid(0x555B63, disabled_brush_.ReleaseAndGetAddressOf());
    }
    if (SUCCEEDED(result)) {
        result = create_solid(0x0A1C2B, card_brush_.ReleaseAndGetAddressOf());
    }
    if (FAILED(result)) {
        discard_device_resources();
    }
    return result;
}

void LauncherRenderer::resize(const UINT width, const UINT height) noexcept {
    if (render_target_ && width != 0U && height != 0U) {
        const HRESULT result = render_target_->Resize(D2D1::SizeU(width, height));
        if (result == D2DERR_RECREATE_TARGET) {
            discard_device_resources();
            InvalidateRect(window_, nullptr, FALSE);
        }
    }
}

void LauncherRenderer::set_dpi(const UINT dpi) noexcept {
    dpi_ = dpi == 0U ? 96U : dpi;
    if (render_target_) {
        render_target_->SetDpi(static_cast<FLOAT>(dpi_), static_cast<FLOAT>(dpi_));
    }
}

HRESULT LauncherRenderer::draw_text(
    const std::wstring_view text,
    IDWriteTextFormat* const format,
    const RectF rectangle,
    ID2D1Brush* const brush,
    const DWRITE_WORD_WRAPPING wrapping,
    const bool ellipsis) noexcept {
    if (text.size() > std::numeric_limits<UINT32>::max()) {
        return E_INVALIDARG;
    }
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    const wchar_t* const characters = text.empty() ? L"" : text.data();
    HRESULT result = dwrite_factory_->CreateTextLayout(
        characters,
        static_cast<UINT32>(text.size()),
        format,
        rectangle.right - rectangle.left,
        rectangle.bottom - rectangle.top,
        layout.GetAddressOf());
    if (FAILED(result)) {
        return result;
    }
    result = layout->SetWordWrapping(wrapping);
    if (SUCCEEDED(result) && ellipsis) {
        const DWRITE_TRIMMING trimming{
            DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0U, 0U};
        result = layout->SetTrimming(&trimming, ellipsis_.Get());
    }
    if (SUCCEEDED(result)) {
        render_target_->DrawTextLayout(
            D2D1::Point2F(rectangle.left, rectangle.top),
            layout.Get(),
            brush,
            D2D1_DRAW_TEXT_OPTIONS_NONE);
    }
    return result;
}

void LauncherRenderer::draw_button(
    const RectF rectangle,
    const std::wstring_view label,
    const LauncherButton button,
    const LauncherView& view) noexcept {
    const bool disabled = button == LauncherButton::start && !view.start_enabled;
    const bool hovered = !disabled && view.hovered_button == button;
    const bool pressed = !disabled && view.pressed_button == button;
    const bool focused = !disabled && view.focused_button == button;

    ID2D1Brush* fill = disabled
        ? static_cast<ID2D1Brush*>(disabled_brush_.Get())
        : static_cast<ID2D1Brush*>(
              pressed ? pressed_brush_.Get() : button_brush_.Get());
    ID2D1Brush* border = hovered || focused
        ? static_cast<ID2D1Brush*>(cyan_brush_.Get())
        : static_cast<ID2D1Brush*>(silver_brush_.Get());
    const D2D1_ROUNDED_RECT shape = rounded_rectangle(rectangle);
    render_target_->FillRoundedRectangle(shape, fill);
    render_target_->DrawRoundedRectangle(shape, border, hovered || focused ? 2.5F : 1.5F);
    draw_text(
        label,
        button_format_.Get(),
        rectangle,
        white_brush_.Get(),
        DWRITE_WORD_WRAPPING_NO_WRAP);
}

HRESULT LauncherRenderer::render(const LauncherView& view) noexcept {
    HRESULT result = ensure_render_target();
    if (FAILED(result)) {
        return result;
    }

    render_target_->BeginDraw();
    render_target_->SetTransform(D2D1::Matrix3x2F::Identity());
    const D2D1_SIZE_F target_size = render_target_->GetSize();
    background_brush_->SetEndPoint(D2D1::Point2F(target_size.width, target_size.height));
    render_target_->FillRectangle(
        D2D1::RectF(0.0F, 0.0F, target_size.width, target_size.height),
        background_brush_.Get());

    result = draw_text(
        L"无限地下城",
        title_format_.Get(),
        view.layout.title,
        white_brush_.Get(),
        DWRITE_WORD_WRAPPING_NO_WRAP);
    if (SUCCEEDED(result)) {
        result = draw_text(
            L"版本 0.1.0 · 单人无限地下城",
            subtitle_format_.Get(),
            view.layout.subtitle,
            white_brush_.Get(),
            DWRITE_WORD_WRAPPING_NO_WRAP);
    }

    render_target_->DrawRoundedRectangle(
        rounded_rectangle(view.layout.status), silver_brush_.Get(), 1.5F);
    if (SUCCEEDED(result)) {
        result = draw_text(
            view.status_text,
            status_format_.Get(),
            view.layout.status,
            view.status_ready ? static_cast<ID2D1Brush*>(green_brush_.Get())
                              : static_cast<ID2D1Brush*>(red_brush_.Get()),
            DWRITE_WORD_WRAPPING_NO_WRAP);
    }

    draw_button(
        view.layout.start, L"开始游戏", LauncherButton::start, view);
    draw_button(
        view.layout.verify, L"检查游戏文件", LauncherButton::verify, view);
    draw_button(
        view.layout.save, L"打开存档目录", LauncherButton::save, view);
    draw_button(view.layout.exit, L"退出", LauncherButton::exit, view);

    render_target_->DrawRoundedRectangle(
        rounded_rectangle(view.layout.path), silver_brush_.Get(), 1.5F);
    if (SUCCEEDED(result)) {
        const RectF path_content{
            view.layout.path.left + 12.0F,
            view.layout.path.top,
            view.layout.path.right - 12.0F,
            view.layout.path.bottom,
        };
        result = draw_text(
            view.path_text,
            path_format_.Get(),
            path_content,
            white_brush_.Get(),
            DWRITE_WORD_WRAPPING_NO_WRAP,
            true);
    }

    if (view.show_full_path) {
        const D2D1_ROUNDED_RECT card = rounded_rectangle(kTooltipRectangle);
        render_target_->FillRoundedRectangle(card, card_brush_.Get());
        render_target_->DrawRoundedRectangle(card, cyan_brush_.Get(), 2.0F);
        const RectF tooltip_content{
            kTooltipRectangle.left + 12.0F,
            kTooltipRectangle.top + 8.0F,
            kTooltipRectangle.right - 12.0F,
            kTooltipRectangle.bottom - 8.0F,
        };
        if (SUCCEEDED(result)) {
            result = draw_text(
                view.full_path,
                path_format_.Get(),
                tooltip_content,
                white_brush_.Get(),
                DWRITE_WORD_WRAPPING_WRAP);
        }
    }

    const HRESULT end_result = render_target_->EndDraw();
    if (end_result == D2DERR_RECREATE_TARGET) {
        discard_device_resources();
        InvalidateRect(window_, nullptr, FALSE);
        return S_OK;
    }
    return FAILED(result) ? result : end_result;
}

void LauncherRenderer::discard_device_resources() noexcept {
    card_brush_.Reset();
    disabled_brush_.Reset();
    pressed_brush_.Reset();
    button_brush_.Reset();
    cyan_brush_.Reset();
    silver_brush_.Reset();
    red_brush_.Reset();
    green_brush_.Reset();
    white_brush_.Reset();
    background_brush_.Reset();
    render_target_.Reset();
}

}  // namespace arpg::launcher
