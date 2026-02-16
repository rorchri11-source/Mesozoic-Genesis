#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <array>
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>

namespace Mesozoic {
namespace Graphics {

struct WindowConfig {
  uint32_t width = 1280;
  uint32_t height = 720;
  std::string title = "Mesozoic Genesis";
  bool fullscreen = false;
  bool resizable = true;
};

class Window {
public:
  HWND handle = nullptr;
  HINSTANCE hInstance = nullptr;
  WindowConfig config;
  // Mouse Button Constants
  static constexpr int MOUSE_LEFT = 0;
  static constexpr int MOUSE_RIGHT = 1;
  static constexpr int MOUSE_MIDDLE = 2;

  std::array<bool, 8> mouseButtons; // Track mouse buttons

  Window() {
    keys.fill(false);
    mouseButtons.fill(false);
  }

  // ... (Initialize remains check)

  // ... (Cleanup remains check)

  // ... (PollEvents remains check)

  // ... (ShouldClose/IsMinimized check)

  bool IsKeyPressed(int key) const {
    if (key < 0 || key >= 512)
      return false;
    return keys[key];
  }

  bool IsMouseButtonDown(int button) const {
    if (button < 0 || button >= 8)
      return false;
    return mouseButtons[button];
  }

  void GetMousePosition(float &x, float &y) const {
    // Return absolute screen coordinates (client space)
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(handle, &pt);
    x = static_cast<float>(pt.x);
    y = static_cast<float>(pt.y);
    // Clamp to window
    if (x < 0)
      x = 0;
    if (y < 0)
      y = 0;
    if (x > config.width)
      x = (float)config.width;
    if (y > config.height)
      y = (float)config.height;
  }

  // ... (SetTitle / SetCursorLocked / GetMouseDelta check)

  // ... (GetRequiredVulkanExtensions check)

private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                  LPARAM lParam) {
    Window *window = nullptr;
    if (msg == WM_NCCREATE) {
      LPCREATESTRUCT create_struct = (LPCREATESTRUCT)lParam;
      window = (Window *)create_struct->lpCreateParams;
      SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)window);
    } else {
      window = (Window *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (window) {
      switch (msg) {
      case WM_CLOSE:
        window->shouldClose = true;
        PostQuitMessage(0);
        return 0;
      case WM_KEYDOWN:
        if (wParam < 512)
          window->keys[wParam] = true;
        if (wParam == VK_ESCAPE)
          window->shouldClose = true;
        return 0;
      case WM_KEYUP:
        if (wParam < 512)
          window->keys[wParam] = false;
        return 0;
      case WM_LBUTTONDOWN:
        window->mouseButtons[MOUSE_LEFT] = true;
        return 0;
      case WM_LBUTTONUP:
        window->mouseButtons[MOUSE_LEFT] = false;
        return 0;
      case WM_RBUTTONDOWN:
        window->mouseButtons[MOUSE_RIGHT] = true;
        return 0;
      case WM_RBUTTONUP:
        window->mouseButtons[MOUSE_RIGHT] = false;
        return 0;
      case WM_MBUTTONDOWN:
        window->mouseButtons[MOUSE_MIDDLE] = true;
        return 0;
      case WM_MBUTTONUP:
        window->mouseButtons[MOUSE_MIDDLE] = false;
        return 0;
      case WM_MOUSEMOVE: {
        int x = (short)LOWORD(lParam);
        int y = (short)HIWORD(lParam);

        // Always update absolute position tracking
        // (Though GetMousePosition queries OS directly for fresh data)

        if (window->cursorLocked) {
          window->mouseDeltaX += (float)(x - window->lastMouseX);
          window->mouseDeltaY += (float)(y - window->lastMouseY);
          // Center mouse back to avoid hitting edges
          POINT pt = {static_cast<LONG>(window->config.width / 2),
                      static_cast<LONG>(window->config.height / 2)};
          window->lastMouseX = pt.x;
          window->lastMouseY = pt.y;
          ClientToScreen(hwnd, &pt);
          SetCursorPos(pt.x, pt.y);
        } else {
          window->lastMouseX = x;
          window->lastMouseY = y;
        }
        return 0;
      }
      }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }
};

} // namespace Graphics
} // namespace Mesozoic
