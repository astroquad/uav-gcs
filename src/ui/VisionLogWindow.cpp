#include "ui/VisionLogWindow.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <memory>
#include <cstddef>
#include <algorithm>
#include <string>
#include <utility>

namespace gcs::ui {
namespace {

#ifdef _WIN32

constexpr wchar_t kWindowClassName[] = L"AstroquadVisionLogWindow";

struct VisionLogWindowState {
    HWND hwnd = nullptr;
    HWND grid_edit = nullptr;
    HWND markers_edit = nullptr;  // Cycle 23
    HWND detail_edit = nullptr;
    bool closed = false;
};

std::string normalizeLineEndings(const std::string& text)
{
    std::string output;
    output.reserve(text.size() + 16);
    for (std::size_t index = 0; index < text.size(); ++index) {
        const char ch = text[index];
        if (ch == '\n' && (index == 0 || text[index - 1] != '\r')) {
            output.push_back('\r');
        }
        output.push_back(ch);
    }
    return output;
}

std::wstring widen(const std::string& text)
{
    const std::string normalized = normalizeLineEndings(text);
    if (normalized.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(
        CP_UTF8,
        0,
        normalized.c_str(),
        static_cast<int>(normalized.size()),
        nullptr,
        0);
    if (length <= 0) {
        return std::wstring(normalized.begin(), normalized.end());
    }

    std::wstring output(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        0,
        normalized.c_str(),
        static_cast<int>(normalized.size()),
        output.data(),
        length);
    return output;
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto* state = reinterpret_cast<VisionLogWindowState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        state = reinterpret_cast<VisionLogWindowState*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }

    switch (message) {
    case WM_CREATE:
        if (state != nullptr) {
            const DWORD common_style = WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                                       ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL |
                                       ES_READONLY;
            state->grid_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                common_style, 0, 0, 0, 0,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            // Cycle 23: third panel for the discovered-marker registry.
            state->markers_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                common_style, 0, 0, 0, 0,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            state->detail_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                common_style, 0, 0, 0, 0,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            const HFONT font = reinterpret_cast<HFONT>(GetStockObject(ANSI_FIXED_FONT));
            SendMessageW(state->grid_edit,    WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            SendMessageW(state->markers_edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            SendMessageW(state->detail_edit,  WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }
        return 0;
    case WM_SIZE:
        if (state != nullptr && state->grid_edit != nullptr &&
            state->markers_edit != nullptr && state->detail_edit != nullptr) {
            // Cycle 23: 3-panel layout.
            //   [grid (top-left, ~75% width)] [markers (top-right, ~25%)]
            //   [detail (bottom)]
            // Top section is ~50% of the window height so a 5x8 grid (~17
            // rows) fits without scrolling on the default 1200x800 window.
            const int width = LOWORD(lparam);
            const int height = HIWORD(lparam);
            const int gap = 2;
            const int top_h = std::clamp(height / 2, 240, 520);
            const int top_h_clamped = std::min(top_h, height);
            const int markers_w = std::clamp(width / 4, 200, 360);
            const int grid_w = std::max(0, width - markers_w - gap);
            MoveWindow(state->grid_edit,    0,                  0, grid_w,    top_h_clamped, TRUE);
            MoveWindow(state->markers_edit, grid_w + gap,       0, markers_w, top_h_clamped, TRUE);
            MoveWindow(state->detail_edit,  0,            top_h_clamped + gap,
                                            width,        std::max(0, height - top_h_clamped - gap),
                                            TRUE);
        }
        return 0;
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
            state->grid_edit = nullptr;
            state->markers_edit = nullptr;
            state->detail_edit = nullptr;
        }
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;
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
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    wc.lpszClassName = kWindowClassName;
    RegisterClassW(&wc);
    registered = true;
}

#else

struct VisionLogWindowState {
    bool closed = true;
};

#endif

} // namespace

VisionLogWindow::VisionLogWindow(std::string title)
{
    auto state = std::make_unique<VisionLogWindowState>();
#ifdef _WIN32
    registerWindowClass();
    state->hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        widen(std::move(title)).c_str(),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        // Cycle 23: bumped 640x360 -> 1200x800 so the new 3-panel layout
        // (grid + markers + detail) has room for a 5x8 grid + sidebar.
        1200,
        800,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        state.get());
#else
    (void)title;
#endif
    native_state_ = state.release();
}

VisionLogWindow::~VisionLogWindow()
{
    auto* state = static_cast<VisionLogWindowState*>(native_state_);
    if (state == nullptr) {
        return;
    }
#ifdef _WIN32
    if (state->hwnd != nullptr) {
        DestroyWindow(state->hwnd);
    }
#endif
    delete state;
    native_state_ = nullptr;
}

bool VisionLogWindow::update(const std::string& text)
{
    return update({}, {}, text);
}

bool VisionLogWindow::update(const std::string& grid_text, const std::string& detail_text)
{
    return update(grid_text, {}, detail_text);
}

bool VisionLogWindow::update(const std::string& grid_text,
                             const std::string& markers_text,
                             const std::string& detail_text)
{
    auto* state = static_cast<VisionLogWindowState*>(native_state_);
    if (state == nullptr || state->closed) {
        return false;
    }
#ifdef _WIN32
    if (state->grid_edit == nullptr || state->markers_edit == nullptr ||
        state->detail_edit == nullptr) {
        return false;
    }
    SetWindowTextW(state->grid_edit,    widen(grid_text).c_str());
    SetWindowTextW(state->markers_edit, widen(markers_text).c_str());
    SetWindowTextW(state->detail_edit,  widen(detail_text).c_str());
    return true;
#else
    (void)grid_text;
    (void)markers_text;
    (void)detail_text;
    return false;
#endif
}

void VisionLogWindow::poll()
{
#ifdef _WIN32
    MSG message {};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
#endif
}

bool VisionLogWindow::available() const
{
    const auto* state = static_cast<const VisionLogWindowState*>(native_state_);
    if (state == nullptr || state->closed) {
        return false;
    }
#ifdef _WIN32
    return state->hwnd != nullptr &&
        state->grid_edit != nullptr &&
        state->markers_edit != nullptr &&
        state->detail_edit != nullptr;
#else
    return false;
#endif
}

} // namespace gcs::ui
