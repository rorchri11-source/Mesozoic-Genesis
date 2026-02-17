#!/bin/bash

# Mesozoic Genesis - Build & Run (Linux)

echo "==========================================="
echo "Mesozoic Genesis - Build & Run (Linux)"
echo "==========================================="

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    mkdir build
fi

cd build

# Run CMake
echo "[1/3] Generating Project Files..."
# Check if running in a headless environment (like CI/Cloud shell)
# If DISPLAY is not set, default to Headless build to avoid glfw errors
if [ -z "$DISPLAY" ]; then
    echo "No DISPLAY detected. Configuring HEADLESS build..."
    cmake .. -DBUILD_HEADLESS=ON
else
    cmake ..
fi

if [ $? -ne 0 ]; then
    echo "[ERROR] CMake generation failed!"
    exit 1
fi

# Build
echo "[2/3] Compiling Project..."
cmake --build .
if [ $? -ne 0 ]; then
    echo "[ERROR] Compilation failed!"
    exit 1
fi

# Run
echo "[3/3] Launching Game..."
if [ -f "./MesozoicGenesis" ]; then
    ./MesozoicGenesis
else
    echo "[ERROR] Executable not found!"
    exit 1
fi
