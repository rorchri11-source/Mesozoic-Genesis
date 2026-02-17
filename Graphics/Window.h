#pragma once

#include <array>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Mesozoic {
namespace Graphics {

struct WindowConfig {
  uint32_t width = 1280;
  uint32_t height = 720;
  std::string title = "Mesozoic Genesis";
  bool fullscreen = false;
  bool resizable = true;
};

#ifdef _WIN32
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
  std::array<bool, 512> keys;
  bool shouldClose = false;
  bool cursorLocked = false;
  float lastMouseX = 0, lastMouseY = 0;
  float mouseDeltaX = 0, mouseDeltaY = 0;

  Window() {
    keys.fill(false);
    mouseButtons.fill(false);
  }

  bool Initialize(const WindowConfig& cfg) {
    config = cfg;
    hInstance = GetModuleHandle(nullptr);

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "MesozoicWindowClass";

    RegisterClassEx(&wc);

    RECT wr = {0, 0, (LONG)config.width, (LONG)config.height};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    handle = CreateWindowEx(0, "MesozoicWindowClass", config.title.c_str(),
                            WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
                            CW_USEDEFAULT, wr.right - wr.left,
                            wr.bottom - wr.top, nullptr, nullptr, hInstance,
                            this);

    if (!handle) {
      std::cerr << "Failed to create window!" << std::endl;
      return false;
    }

    return true;
  }

  void Cleanup() {
    if (handle) {
      DestroyWindow(handle);
      handle = nullptr;
    }
    UnregisterClass("MesozoicWindowClass", hInstance);
  }

  void PollEvents() {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        shouldClose = true;
      }
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  bool ShouldClose() const { return shouldClose; }

  void SetTitle(const std::string& title) {
    SetWindowTextA(handle, title.c_str());
  }

  void SetCursorLocked(bool locked) {
    cursorLocked = locked;
    ShowCursor(!locked);
    if (locked) {
      // Center cursor
      POINT pt = {static_cast<LONG>(config.width / 2),
                  static_cast<LONG>(config.height / 2)};
      lastMouseX = (float)pt.x;
      lastMouseY = (float)pt.y;
      ClientToScreen(handle, &pt);
      SetCursorPos(pt.x, pt.y);
    }
  }

  void GetMouseDelta(float& dx, float& dy) {
    dx = mouseDeltaX;
    dy = mouseDeltaY;
    mouseDeltaX = 0;
    mouseDeltaY = 0;
  }

  std::vector<const char*> GetRequiredVulkanExtensions() {
    return { "VK_KHR_surface", "VK_KHR_win32_surface" };
  }

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
#else
// Mock implementation for Linux/Headless Analysis
class Window {
public:
  WindowConfig config;
  // Mouse Button Constants
  static constexpr int MOUSE_LEFT = 0;
  static constexpr int MOUSE_RIGHT = 1;
  static constexpr int MOUSE_MIDDLE = 2;

  bool shouldClose = false;

  bool Initialize(const WindowConfig& cfg) {
    config = cfg;
    std::cout << "[Window] Initialized HEADLESS mode (Linux)." << std::endl;
    return true;
  }
  void Cleanup() {}
  bool ShouldClose() const { return shouldClose; }
  void PollEvents() {
    // Simulate game loop running for a few frames then exit for testing purposes?
    // Or just let it run until killed?
    // Main.cpp loops until ShouldClose() is true.
    // If we want to run analysis, we might want to let it run briefly.
    // But for now, let's just let it run.
    // We can simulate an ESC key press after 100 frames if we wanted.
  }
  void SetTitle(const std::string&) {}
  void SetCursorLocked(bool) {}
  void GetMouseDelta(float& dx, float& dy) { dx = 0; dy = 0; }
  std::vector<const char*> GetRequiredVulkanExtensions() { return {}; }
  bool IsKeyPressed(int key) const {
      // Simulate ESC (27) to close if needed?
      return false;
  }
  bool IsMouseButtonDown(int) const { return false; }
  void GetMousePosition(float &x, float &y) const { x = 0; y = 0; }
};
#endif

} // namespace Graphics
} // namespace Mesozoic
