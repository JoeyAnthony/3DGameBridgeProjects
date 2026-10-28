/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "displayxrOverlayWindow.h"

#include <cwchar>

static const wchar_t* overlay_class_name = L"GameBridgeDisplayXROverlay";

// WinEvent callbacks carry no user data; they run on the overlay thread, which serves exactly one overlay.
static thread_local DisplayXROverlayWindow* thread_overlay = nullptr;

bool DisplayXROverlayWindow::is_overlay_window(HWND hwnd) {
    wchar_t class_name[64] = {};
    return hwnd != nullptr && GetClassNameW(hwnd, class_name, 64) > 0 && wcscmp(class_name, overlay_class_name) == 0;
}

DisplayXROverlayWindow::~DisplayXROverlayWindow() {
    if (thread != nullptr) {
        CloseHandle(thread);
    }
    if (ready_event != nullptr) {
        CloseHandle(ready_event);
    }
}

HWND DisplayXROverlayWindow::create(HWND game_window) {
    game_hwnd = game_window;
    ready_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (ready_event == nullptr) {
        return nullptr;
    }

    // The thread runs code from this DLL, so it holds a reference on it and releases it as the very last thing it
    // does (FreeLibraryAndExitThread). The add-on can then never be unloaded under a running overlay thread.
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR>(&DisplayXROverlayWindow::thread_main), &module)) {
        return nullptr;
    }
    thread = CreateThread(nullptr, 0, &DisplayXROverlayWindow::thread_main, this, 0, &thread_id);
    if (thread == nullptr) {
        FreeLibrary(module);
        return nullptr;
    }

    // Creating a window owned by the game window sends messages to the game window's thread, which is often this
    // one: keep handling sent messages while waiting (posted messages are left for the game's own loop).
    // The timeout guards against a hung owner window.
    const ULONGLONG deadline = GetTickCount64() + 5000;
    for (ULONGLONG now = GetTickCount64(); now < deadline; now = GetTickCount64()) {
        const DWORD result = MsgWaitForMultipleObjectsEx(1, &ready_event, static_cast<DWORD>(deadline - now), QS_SENDMESSAGE, 0);
        if (result != WAIT_OBJECT_0 + 1) {
            break;
        }
        MSG msg;
        PeekMessageW(&msg, nullptr, 0, 0, PM_NOREMOVE | PM_QS_SENDMESSAGE);
    }
    return overlay_hwnd.load();
}

void DisplayXROverlayWindow::destroy(bool wait) {
    HWND hwnd = overlay_hwnd.exchange(nullptr);
    if (hwnd != nullptr) {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }
    else if (thread_id != 0) {
        PostThreadMessageW(thread_id, WM_QUIT, 0, 0);
    }
    if (wait && thread != nullptr) {
        WaitForSingleObject(thread, 2000);
    }
}

void DisplayXROverlayWindow::set_visible(bool visible) {
    HWND hwnd = overlay_hwnd.load();
    if (hwnd != nullptr) {
        ShowWindowAsync(hwnd, visible ? SW_SHOWNA : SW_HIDE);
    }
}

void DisplayXROverlayWindow::follow_game_window() {
    HWND hwnd = overlay_hwnd.load();
    if (hwnd == nullptr || IsIconic(game_hwnd)) {
        // A minimized owner hides its owned popups by itself.
        return;
    }
    RECT client;
    if (!GetClientRect(game_hwnd, &client) || client.right <= 0 || client.bottom <= 0) {
        return;
    }
    POINT origin = { 0, 0 };
    ClientToScreen(game_hwnd, &origin);
    SetWindowPos(hwnd, nullptr, origin.x, origin.y, client.right, client.bottom, SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT CALLBACK DisplayXROverlayWindow::window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_NCHITTEST:
        // Let every click fall through to the game window underneath.
        return HTTRANSPARENT;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

void CALLBACK DisplayXROverlayWindow::win_event_proc(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG id_object, LONG id_child, DWORD, DWORD) {
    DisplayXROverlayWindow* self = thread_overlay;
    if (self == nullptr || hwnd != self->game_hwnd || id_object != OBJID_WINDOW || id_child != CHILDID_SELF) {
        return;
    }
    HWND overlay = self->overlay_hwnd.load();
    switch (event) {
    case EVENT_OBJECT_LOCATIONCHANGE:
        self->follow_game_window();
        break;
    case EVENT_SYSTEM_MOVESIZESTART:
    case EVENT_SYSTEM_MOVESIZEEND:
        // The display processor watches its own window for the move/size bracket to keep the weave aligned with
        // the lenses during a drag. The user drags the game window, so forward the bracket to the overlay.
        if (overlay != nullptr) {
            SendMessageW(overlay, event == EVENT_SYSTEM_MOVESIZESTART ? WM_ENTERSIZEMOVE : WM_EXITSIZEMOVE, 0, 0);
        }
        break;
    default:
        break;
    }
}

DWORD WINAPI DisplayXROverlayWindow::thread_main(LPVOID param) {
    auto* self = static_cast<DisplayXROverlayWindow*>(param);
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&DisplayXROverlayWindow::thread_main), &module);
    thread_overlay = self;

    // Work in physical pixels whatever the game's DPI awareness is: the runtime sizes its output from this window,
    // and a DPI-virtualized size would weave at the wrong resolution. Resolved at runtime to keep loading on Windows 7.
    using set_dpi_context_fn = DPI_AWARENESS_CONTEXT(WINAPI*)(DPI_AWARENESS_CONTEXT);
    auto set_dpi_context = reinterpret_cast<set_dpi_context_fn>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetThreadDpiAwarenessContext"));
    if (set_dpi_context != nullptr) {
        set_dpi_context(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = &DisplayXROverlayWindow::window_proc;
    wc.hInstance = module;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = overlay_class_name;
    RegisterClassExW(&wc);

    RECT client = {};
    GetClientRect(self->game_hwnd, &client);
    POINT origin = { 0, 0 };
    ClientToScreen(self->game_hwnd, &origin);

    // NOREDIRECTIONBITMAP: the window shows only what the runtime presents into it, never an opaque DWM surface.
    // Owned by the game window, so it stays above it, minimizes with it and goes away with it.
    HWND hwnd = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
                                overlay_class_name, L"3DGameBridge DisplayXR", WS_POPUP | WS_VISIBLE,
                                origin.x, origin.y, client.right > 0 ? client.right : 1, client.bottom > 0 ? client.bottom : 1,
                                self->game_hwnd, nullptr, module, nullptr);
    self->overlay_hwnd.store(hwnd);

    HWINEVENTHOOK hooks[2] = {};
    if (hwnd != nullptr) {
        const DWORD pid = GetCurrentProcessId();
        hooks[0] = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr, &DisplayXROverlayWindow::win_event_proc, pid, 0, WINEVENT_OUTOFCONTEXT);
        hooks[1] = SetWinEventHook(EVENT_SYSTEM_MOVESIZESTART, EVENT_SYSTEM_MOVESIZEEND, nullptr, &DisplayXROverlayWindow::win_event_proc, pid, 0, WINEVENT_OUTOFCONTEXT);
    }
    SetEvent(self->ready_event);

    if (hwnd != nullptr) {
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    for (HWINEVENTHOOK hook : hooks) {
        if (hook != nullptr) {
            UnhookWinEvent(hook);
        }
    }
    UnregisterClassW(overlay_class_name, module);
    thread_overlay = nullptr;
    FreeLibraryAndExitThread(module, 0);
}
