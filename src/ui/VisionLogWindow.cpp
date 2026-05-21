#include "ui/VisionLogWindow.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
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
constexpr int kSplitterSize = 6;
constexpr int kMinTopPaneHeight = 120;
constexpr int kMinDetailPaneHeight = 120;
constexpr int kMinGridPaneWidth = 180;
constexpr int kMinMarkersPaneWidth = 160;

enum class DragMode {
    None,
    VerticalSplitter,
    HorizontalSplitter,
};

struct VisionLogWindowState {
    HWND hwnd = nullptr;
    HWND grid_edit = nullptr;
    HWND markers_edit = nullptr;  // Cycle 23
    HWND detail_edit = nullptr;
    bool closed = false;
    bool layout_initialized = false;
    int top_height = 0;
    int markers_width = 0;
    DragMode drag_mode = DragMode::None;
};

int clampTopHeight(int requested, int height)
{
    if (height <= kSplitterSize) {
        return std::max(0, height);
    }
    const int max_top = std::max(kMinTopPaneHeight,
                                 height - kSplitterSize - kMinDetailPaneHeight);
    return std::clamp(requested, kMinTopPaneHeight, max_top);
}

int clampMarkersWidth(int requested, int width)
{
    if (width <= kSplitterSize) {
        return std::max(0, width);
    }
    const int max_markers = std::max(kMinMarkersPaneWidth,
                                     width - kSplitterSize - kMinGridPaneWidth);
    return std::clamp(requested, kMinMarkersPaneWidth, max_markers);
}

void layoutControls(VisionLogWindowState* state, int width, int height)
{
    if (state == nullptr ||
        state->grid_edit == nullptr ||
        state->markers_edit == nullptr ||
        state->detail_edit == nullptr) {
        return;
    }

    if (!state->layout_initialized) {
        state->top_height = std::clamp(height / 2, 240, 520);
        state->markers_width = std::clamp(width / 4, 200, 360);
        state->layout_initialized = true;
    }

    state->top_height = clampTopHeight(state->top_height, height);
    state->markers_width = clampMarkersWidth(state->markers_width, width);

    const int grid_w = std::max(0, width - state->markers_width - kSplitterSize);
    const int detail_y = state->top_height + kSplitterSize;
    MoveWindow(state->grid_edit,    0,                    0,
               grid_w,             state->top_height,     TRUE);
    MoveWindow(state->markers_edit, grid_w + kSplitterSize, 0,
               state->markers_width, state->top_height,   TRUE);
    MoveWindow(state->detail_edit,  0,                    detail_y,
               width,              std::max(0, height - detail_y),
               TRUE);
}

DragMode hitTestSplitter(const VisionLogWindowState* state, int x, int y, int width)
{
    if (state == nullptr || !state->layout_initialized) {
        return DragMode::None;
    }
    const int grid_w = std::max(0, width - state->markers_width - kSplitterSize);
    const bool over_vertical =
        y >= 0 && y < state->top_height &&
        x >= grid_w && x < grid_w + kSplitterSize;
    if (over_vertical) {
        return DragMode::VerticalSplitter;
    }
    const bool over_horizontal =
        y >= state->top_height && y < state->top_height + kSplitterSize;
    if (over_horizontal) {
        return DragMode::HorizontalSplitter;
    }
    return DragMode::None;
}

void clientSize(HWND hwnd, int& width, int& height)
{
    RECT rect {};
    GetClientRect(hwnd, &rect);
    width = std::max(0L, rect.right - rect.left);
    height = std::max(0L, rect.bottom - rect.top);
}

void setSplitterCursor(DragMode mode)
{
    if (mode == DragMode::VerticalSplitter) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
    } else if (mode == DragMode::HorizontalSplitter) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
    } else {
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    }
}

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
            layoutControls(state, LOWORD(lparam), HIWORD(lparam));
        }
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lparam) == HTCLIENT && state != nullptr) {
            POINT point {};
            GetCursorPos(&point);
            ScreenToClient(hwnd, &point);
            int width = 0;
            int height = 0;
            clientSize(hwnd, width, height);
            (void)height;
            const DragMode hit = state->drag_mode != DragMode::None
                ? state->drag_mode
                : hitTestSplitter(state, point.x, point.y, width);
            if (hit != DragMode::None) {
                setSplitterCursor(hit);
                return TRUE;
            }
        }
        break;
    case WM_LBUTTONDOWN:
        if (state != nullptr) {
            int width = 0;
            int height = 0;
            clientSize(hwnd, width, height);
            const int x = GET_X_LPARAM(lparam);
            const int y = GET_Y_LPARAM(lparam);
            const DragMode hit = hitTestSplitter(state, x, y, width);
            if (hit != DragMode::None) {
                state->drag_mode = hit;
                SetCapture(hwnd);
                setSplitterCursor(hit);
                return 0;
            }
        }
        break;
    case WM_MOUSEMOVE:
        if (state != nullptr) {
            int width = 0;
            int height = 0;
            clientSize(hwnd, width, height);
            const int x = GET_X_LPARAM(lparam);
            const int y = GET_Y_LPARAM(lparam);
            if (state->drag_mode == DragMode::VerticalSplitter) {
                state->markers_width = clampMarkersWidth(
                    width - x - (kSplitterSize / 2),
                    width);
                layoutControls(state, width, height);
                setSplitterCursor(state->drag_mode);
                return 0;
            }
            if (state->drag_mode == DragMode::HorizontalSplitter) {
                state->top_height = clampTopHeight(y - (kSplitterSize / 2), height);
                layoutControls(state, width, height);
                setSplitterCursor(state->drag_mode);
                return 0;
            }
            setSplitterCursor(hitTestSplitter(state, x, y, width));
        }
        break;
    case WM_LBUTTONUP:
    case WM_CANCELMODE:
        if (state != nullptr && state->drag_mode != DragMode::None) {
            state->drag_mode = DragMode::None;
            ReleaseCapture();
            return 0;
        }
        break;
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
