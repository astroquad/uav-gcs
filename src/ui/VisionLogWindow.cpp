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
    HWND detail_edit = nullptr;
    bool closed = false;
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
            state->grid_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE |
                    ES_AUTOVSCROLL | ES_READONLY,
                0,
                0,
                0,
                0,
                hwnd,
                nullptr,
                GetModuleHandleW(nullptr),
                nullptr);
            state->detail_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE |
                    ES_AUTOVSCROLL | ES_READONLY,
                0,
                0,
                0,
                0,
                hwnd,
                nullptr,
                GetModuleHandleW(nullptr),
                nullptr);
            SendMessageW(state->grid_edit, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(ANSI_FIXED_FONT)), TRUE);
            SendMessageW(state->detail_edit, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(ANSI_FIXED_FONT)), TRUE);
        }
        return 0;
    case WM_SIZE:
        if (state != nullptr && state->grid_edit != nullptr && state->detail_edit != nullptr) {
            const int width = LOWORD(lparam);
            const int height = HIWORD(lparam);
            const int grid_height = std::clamp(height / 3, 120, 260);
            MoveWindow(
                state->grid_edit,
                0,
                0,
                width,
                std::min(grid_height, height),
                TRUE);
            MoveWindow(
                state->detail_edit,
                0,
                std::min(grid_height, height),
                width,
                std::max(0, height - grid_height),
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
        640,
        360,
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
    return update({}, text);
}

bool VisionLogWindow::update(const std::string& grid_text, const std::string& detail_text)
{
    auto* state = static_cast<VisionLogWindowState*>(native_state_);
    if (state == nullptr || state->closed) {
        return false;
    }
#ifdef _WIN32
    if (state->grid_edit == nullptr || state->detail_edit == nullptr) {
        return false;
    }
    const std::wstring wide_grid_text = widen(grid_text);
    const std::wstring wide_detail_text = widen(detail_text);
    SetWindowTextW(state->grid_edit, wide_grid_text.c_str());
    SetWindowTextW(state->detail_edit, wide_detail_text.c_str());
    return true;
#else
    (void)grid_text;
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
        state->detail_edit != nullptr;
#else
    return false;
#endif
}

} // namespace gcs::ui
