#include "ui/VideoWindow.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincodec.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace gcs::ui {
namespace {

constexpr wchar_t kWindowClassName[] = L"AstroquadVideoWindow";

struct VideoWindowState {
    HWND hwnd = nullptr;
    IWICImagingFactory* factory = nullptr;
    bool com_initialized = false;
    bool closed = false;
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> bgra;
    std::wstring status;
    std::wstring overlay;
    std::vector<overlay::OverlayPrimitive> overlays;
};

std::wstring widen(const std::string& text)
{
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(
        CP_UTF8,
        0,
        text.c_str(),
        static_cast<int>(text.size()),
        nullptr,
        0);
    if (length <= 0) {
        return std::wstring(text.begin(), text.end());
    }

    std::wstring output(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        0,
        text.c_str(),
        static_cast<int>(text.size()),
        output.data(),
        length);
    return output;
}

template <typename T>
void releaseCom(T*& object)
{
    if (object != nullptr) {
        object->Release();
        object = nullptr;
    }
}

COLORREF toColorRef(const overlay::Color& color)
{
    return RGB(color.r, color.g, color.b);
}

int scaledX(const overlay::Point2f& point, int draw_x, int draw_width, int image_width)
{
    return draw_x + static_cast<int>(std::lround(point.x * draw_width / std::max(1, image_width)));
}

int scaledY(const overlay::Point2f& point, int draw_y, int draw_height, int image_height)
{
    return draw_y + static_cast<int>(std::lround(point.y * draw_height / std::max(1, image_height)));
}

void drawOverlayLine(
    HDC dc,
    const overlay::OverlayLine& line,
    int draw_x,
    int draw_y,
    int draw_width,
    int draw_height,
    int image_width,
    int image_height)
{
    HPEN pen = CreatePen(
        PS_SOLID,
        std::max(1, line.thickness),
        toColorRef(line.color));
    HGDIOBJ old_pen = SelectObject(dc, pen);
    MoveToEx(
        dc,
        scaledX(line.start, draw_x, draw_width, image_width),
        scaledY(line.start, draw_y, draw_height, image_height),
        nullptr);
    LineTo(
        dc,
        scaledX(line.end, draw_x, draw_width, image_width),
        scaledY(line.end, draw_y, draw_height, image_height));
    SelectObject(dc, old_pen);
    DeleteObject(pen);
}

void drawOverlayCircle(
    HDC dc,
    const overlay::OverlayCircle& circle,
    int draw_x,
    int draw_y,
    int draw_width,
    int draw_height,
    int image_width,
    int image_height)
{
    const int center_x = scaledX(circle.center, draw_x, draw_width, image_width);
    const int center_y = scaledY(circle.center, draw_y, draw_height, image_height);
    const int radius_x = std::max(1, static_cast<int>(std::lround(circle.radius * draw_width / std::max(1, image_width))));
    const int radius_y = std::max(1, static_cast<int>(std::lround(circle.radius * draw_height / std::max(1, image_height))));

    if (circle.thickness < 0) {
        HBRUSH brush = CreateSolidBrush(toColorRef(circle.color));
        HGDIOBJ old_brush = SelectObject(dc, brush);
        HGDIOBJ old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
        Ellipse(dc, center_x - radius_x, center_y - radius_y, center_x + radius_x, center_y + radius_y);
        SelectObject(dc, old_pen);
        SelectObject(dc, old_brush);
        DeleteObject(brush);
        return;
    }

    HPEN pen = CreatePen(
        PS_SOLID,
        std::max(1, circle.thickness),
        toColorRef(circle.color));
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Ellipse(dc, center_x - radius_x, center_y - radius_y, center_x + radius_x, center_y + radius_y);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
}

void drawOverlayText(
    HDC dc,
    const overlay::OverlayText& text,
    int draw_x,
    int draw_y,
    int draw_width,
    int draw_height,
    int image_width,
    int image_height)
{
    SetTextColor(dc, toColorRef(text.color));
    const int x = scaledX(text.origin, draw_x, draw_width, image_width);
    const int y = scaledY(text.origin, draw_y, draw_height, image_height);
    RECT text_rect {x, y - 18, x + 480, y + 42};
    const std::wstring wide_text = widen(text.text);
    DrawTextW(dc, wide_text.c_str(), -1, &text_rect, DT_LEFT | DT_WORDBREAK);
}

void drawOverlays(
    HDC dc,
    const VideoWindowState& state,
    int draw_x,
    int draw_y,
    int draw_width,
    int draw_height)
{
    for (const auto& primitive : state.overlays) {
        switch (primitive.type) {
        case overlay::OverlayPrimitive::Type::Line:
            drawOverlayLine(dc, primitive.line, draw_x, draw_y, draw_width, draw_height, state.width, state.height);
            break;
        case overlay::OverlayPrimitive::Type::Circle:
            drawOverlayCircle(dc, primitive.circle, draw_x, draw_y, draw_width, draw_height, state.width, state.height);
            break;
        case overlay::OverlayPrimitive::Type::Text:
            drawOverlayText(dc, primitive.text, draw_x, draw_y, draw_width, draw_height, state.width, state.height);
            break;
        }
    }
}

void paintWindow(HWND hwnd, VideoWindowState& state)
{
    PAINTSTRUCT paint {};
    HDC dc = BeginPaint(hwnd, &paint);

    RECT client {};
    GetClientRect(hwnd, &client);
    const int client_width = std::max(1L, client.right - client.left);
    const int client_height = std::max(1L, client.bottom - client.top);

    HDC memory_dc = CreateCompatibleDC(dc);
    HBITMAP memory_bitmap = CreateCompatibleBitmap(dc, client_width, client_height);
    HGDIOBJ old_bitmap = SelectObject(memory_dc, memory_bitmap);

    HBRUSH black = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    FillRect(memory_dc, &client, black);

    SetBkMode(memory_dc, TRANSPARENT);

    if (!state.bgra.empty() && state.width > 0 && state.height > 0) {
        BITMAPINFO info {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = state.width;
        info.bmiHeader.biHeight = -state.height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        const double image_aspect = static_cast<double>(state.width) / state.height;
        const double client_aspect = static_cast<double>(client_width) / client_height;
        int draw_width = client_width;
        int draw_height = client_height;
        if (client_aspect > image_aspect) {
            draw_width = static_cast<int>(client_height * image_aspect);
        } else {
            draw_height = static_cast<int>(client_width / image_aspect);
        }
        const int draw_x = (client_width - draw_width) / 2;
        const int draw_y = (client_height - draw_height) / 2;

        StretchDIBits(
            memory_dc,
            draw_x,
            draw_y,
            draw_width,
            draw_height,
            0,
            0,
            state.width,
            state.height,
            state.bgra.data(),
            &info,
            DIB_RGB_COLORS,
            SRCCOPY);

        SetTextColor(memory_dc, RGB(0, 255, 0));
        RECT text_rect {12, 10, client_width - 12, 40};
        DrawTextW(memory_dc, state.overlay.c_str(), -1, &text_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        drawOverlays(memory_dc, state, draw_x, draw_y, draw_width, draw_height);
    } else {
        SetTextColor(memory_dc, RGB(255, 220, 0));
        RECT text_rect {16, 0, client_width - 16, client_height};
        DrawTextW(memory_dc, state.status.c_str(), -1, &text_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    }

    BitBlt(dc, 0, 0, client_width, client_height, memory_dc, 0, 0, SRCCOPY);
    SelectObject(memory_dc, old_bitmap);
    DeleteObject(memory_bitmap);
    DeleteDC(memory_dc);

    EndPaint(hwnd, &paint);
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto* state = reinterpret_cast<VideoWindowState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        state = reinterpret_cast<VideoWindowState*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }

    switch (message) {
    case WM_CLOSE:
        if (state != nullptr) {
            state->closed = true;
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (state != nullptr) {
            state->closed = true;
            state->hwnd = nullptr;
        }
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;
    case WM_KEYDOWN:
        if (state != nullptr && (wparam == VK_ESCAPE || wparam == 'Q')) {
            state->closed = true;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_PAINT:
        if (state != nullptr) {
            paintWindow(hwnd, *state);
            return 0;
        }
        break;
    case WM_ERASEBKGND:
        return 1;
    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

void registerWindowClass()
{
    static bool registered = false;
    if (registered) {
        return;
    }

    WNDCLASSW wc {};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kWindowClassName;
    RegisterClassW(&wc);
    registered = true;
}

bool decodeJpeg(IWICImagingFactory* factory, const std::vector<std::uint8_t>& jpeg, int& width, int& height, std::vector<std::uint8_t>& bgra)
{
    if (factory == nullptr || jpeg.empty()) {
        return false;
    }

    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;

    HRESULT hr = factory->CreateStream(&stream);
    if (SUCCEEDED(hr)) {
        hr = stream->InitializeFromMemory(
            const_cast<BYTE*>(reinterpret_cast<const BYTE*>(jpeg.data())),
            static_cast<DWORD>(jpeg.size()));
    }
    if (SUCCEEDED(hr)) {
        hr = factory->CreateDecoderFromStream(
            stream,
            nullptr,
            WICDecodeMetadataCacheOnLoad,
            &decoder);
    }
    if (SUCCEEDED(hr)) {
        hr = decoder->GetFrame(0, &frame);
    }
    UINT decoded_width = 0;
    UINT decoded_height = 0;
    if (SUCCEEDED(hr)) {
        hr = frame->GetSize(&decoded_width, &decoded_height);
    }
    if (SUCCEEDED(hr)) {
        hr = factory->CreateFormatConverter(&converter);
    }
    if (SUCCEEDED(hr)) {
        hr = converter->Initialize(
            frame,
            GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom);
    }
    if (SUCCEEDED(hr)) {
        std::vector<std::uint8_t> decoded(
            static_cast<std::size_t>(decoded_width) * decoded_height * 4);
        hr = converter->CopyPixels(
            nullptr,
            decoded_width * 4,
            static_cast<UINT>(decoded.size()),
            decoded.data());
        if (SUCCEEDED(hr)) {
            width = static_cast<int>(decoded_width);
            height = static_cast<int>(decoded_height);
            bgra = std::move(decoded);
        }
    }

    releaseCom(converter);
    releaseCom(frame);
    releaseCom(decoder);
    releaseCom(stream);
    return SUCCEEDED(hr);
}

} // namespace

VideoWindow::VideoWindow(std::string title)
    : title_(std::move(title))
{
    auto state = std::make_unique<VideoWindowState>();
    const HRESULT coinit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    state->com_initialized = SUCCEEDED(coinit);
    if (SUCCEEDED(coinit) || coinit == RPC_E_CHANGED_MODE) {
        CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&state->factory));
    }

    state->status = L"waiting for video stream...";
    registerWindowClass();
    state->hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        widen(title_).c_str(),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        800,
        600,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        state.get());

    native_state_ = state.release();
}

VideoWindow::~VideoWindow()
{
    auto* state = static_cast<VideoWindowState*>(native_state_);
    if (state == nullptr) {
        return;
    }
    if (state->hwnd != nullptr) {
        DestroyWindow(state->hwnd);
    }
    releaseCom(state->factory);
    if (state->com_initialized) {
        CoUninitialize();
    }
    delete state;
    native_state_ = nullptr;
}

bool VideoWindow::showFrame(const video::JpegFrame& frame)
{
    return showFrame(frame, {});
}

bool VideoWindow::showFrame(
    const video::JpegFrame& frame,
    const std::vector<overlay::OverlayPrimitive>& overlays)
{
    auto& state = *static_cast<VideoWindowState*>(native_state_);
    if (state.closed) {
        return false;
    }

    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
    if (!decodeJpeg(state.factory, frame.data, width, height, pixels)) {
        return false;
    }

    state.width = width;
    state.height = height;
    state.bgra = std::move(pixels);
    state.overlays = overlays;
    state.overlay = widen(
        "frame " + std::to_string(frame.frame_id));

    if (state.hwnd != nullptr) {
        InvalidateRect(state.hwnd, nullptr, FALSE);
        UpdateWindow(state.hwnd);
    }
    return true;
}

void VideoWindow::showStatus(const std::string& status)
{
    auto& state = *static_cast<VideoWindowState*>(native_state_);
    state.status = widen(status);
    state.bgra.clear();
    state.overlays.clear();
    state.width = 0;
    state.height = 0;
    if (state.hwnd != nullptr) {
        InvalidateRect(state.hwnd, nullptr, TRUE);
        UpdateWindow(state.hwnd);
    }
}

bool VideoWindow::shouldClose(int wait_ms)
{
    auto& state = *static_cast<VideoWindowState*>(native_state_);
    MSG message {};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (wait_ms > 0) {
        Sleep(static_cast<DWORD>(wait_ms));
    }
    return state.closed || state.hwnd == nullptr;
}

} // namespace gcs::ui
