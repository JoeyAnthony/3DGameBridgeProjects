/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once

#include <windows.h>
#include <atomic>

// The window DisplayXR presents the woven image into.
// The game already owns a DXGI swapchain on its own window and a window can only carry one, so the runtime gets a
// window of its own: a borderless popup owned by the game window that covers the game's client area. It never
// activates and is invisible to hit-testing, so keyboard and mouse input keep going to the game.
// The window lives on its own thread with its own message loop, so it never depends on the game pumping messages.
class DisplayXROverlayWindow {
    HWND game_hwnd = nullptr;
    std::atomic<HWND> overlay_hwnd{nullptr};
    HANDLE thread = nullptr;
    DWORD thread_id = 0;
    HANDLE ready_event = nullptr;

    static DWORD WINAPI thread_main(LPVOID param);
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    static void CALLBACK win_event_proc(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG id_object, LONG id_child, DWORD thread, DWORD time);

    /// \brief Moves the overlay over the game's client area. Runs on the overlay thread.
    void follow_game_window();

public:
    ~DisplayXROverlayWindow();

    /// \brief Starts the overlay thread and creates the window over the game window.
    /// \return The overlay window, or nullptr if it could not be created
    HWND create(HWND game_window);

    /// \brief Destroys the window and stops its thread.
    /// \param wait Wait for the thread to finish. Must be false under the loader lock (DllMain).
    void destroy(bool wait);

    /// \brief Shows or hides the overlay without activating it
    void set_visible(bool visible);

    HWND get_hwnd() const { return overlay_hwnd.load(); }

    /// \brief Whether a window is a DisplayXR overlay window
    static bool is_overlay_window(HWND hwnd);
};
