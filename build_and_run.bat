@echo off
echo ===========================================
echo Mesozoic Genesis - Build & Run (Windows)
echo ===========================================

REM Create build directory
if not exist build (
    mkdir build
)

cd build

REM Run CMake
echo [1/3] Generating Project Files...
cmake ..
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake generation failed!
    pause
    exit /b %ERRORLEVEL%
)

REM Build
echo [2/3] Compiling Project...
cmake --build . --config Release
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed!
    pause
    exit /b %ERRORLEVEL%
)

REM Run
echo [3/3] Launching Game...
if exist Release\MesozoicGenesis.exe (
    cd Release
    MesozoicGenesis.exe
) else (
    echo [ERROR] Executable not found in Release folder. Checking Debug...
    if exist Debug\MesozoicGenesis.exe (
        cd Debug
        MesozoicGenesis.exe
    ) else (
        echo [ERROR] Could not find MesozoicGenesis.exe in Debug or Release.
        pause
        exit /b 1
    )
)

cd ..\..
echo ===========================================
echo Done.
pause
