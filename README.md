# Mesozoic Genesis

Mesozoic Genesis is a dinosaur park management and simulation game engine built with C++23 and Vulkan.

## Prerequisites

*   **CMake** (3.20 or newer)
*   **C++ Compiler** with **C++23** support (e.g., MSVC 2022, GCC 13+, Clang 16+)
*   **Vulkan SDK** (Required for Windows with graphics support)
*   **Git** (Required to fetch dependencies)

## Build Instructions

### Windows (Recommended)

This project is primarily designed for Windows due to its reliance on Win32 APIs for window management and Vulkan surface creation.

1.  Open PowerShell or Command Prompt.
2.  Navigate to the project directory.
3.  Create a build directory:
    ```powershell
    mkdir build
    cd build
    ```
4.  Generate project files using CMake:
    ```powershell
    cmake ..
    ```
5.  Build the project:
    ```powershell
    cmake --build . --config Release
    ```

### Linux (Headless Mode)

On Linux, the game runs in "Headless" mode (no window, no graphics) unless GLFW is manually enabled and configured with system dependencies. This is useful for running simulation tests or server-side logic.

1.  Open a terminal.
2.  Navigate to the project directory.
3.  Create a build directory:
    ```bash
    mkdir build
    cd build
    ```
4.  Generate project files:
    ```bash
    cmake ..
    ```
    *Note: If you encounter errors regarding Wayland or X11 while fetching GLFW, you may need to install development packages (e.g., `libwayland-dev`, `libxrandr-dev`, `libx11-dev` on Debian/Ubuntu).*
5.  Build the project:
    ```bash
    cmake --build .
    ```

## Running the Game

### Windows
After a successful build, the executable will be located in the `Release` or `Debug` folder inside your `build` directory.

```powershell
.\Release\MesozoicGenesis.exe
```

### Linux
```bash
./MesozoicGenesis
```

## Controls

The game has three main states: **Menu**, **Playing**, and **Editor**.

### General Controls
*   **ESC**: Pause / Open Menu / Exit
*   **W / S**: Move Camera Forward / Backward
*   **A / D**: Move Camera Left / Right
*   **Q / E**: Move Camera Up / Down
*   **Shift**: Move Faster
*   **Mouse**: Look around (Playing Mode) / Interact (Editor Mode)

### Game Modes
*   **Menu**: Start the game or enter the editor.
*   **Playing**: First-person / Free-camera mode with locked cursor. Mouse movement rotates the camera.
*   **Editor**: Unlocked cursor for UI interaction.
    *   **Right-Click + Drag**: Rotate camera.
    *   **Left-Click**: Interact with UI or paint terrain.

## Running Tests

The project includes a test suite for core systems.

1.  Build the `MesozoicTests` target:
    ```bash
    cmake --build . --target MesozoicTests
    ```
2.  Run the tests:
    *   **Windows**: `.\Debug\MesozoicTests.exe`
    *   **Linux**: `./MesozoicTests`
