@echo off
REM DBC Viewer Windows Build Script
REM Requires: CMake, Qt5 (or Qt6), Visual Studio or MinGW

echo Building DBC Viewer for Windows...
echo.

REM Check for Qt in common locations (set QT_DIR if using custom path)
if "%QT_DIR%"=="" (
    if exist "C:\Qt\6.5.0\msvc2019_64" set QT_DIR=C:\Qt\6.5.0\msvc2019_64
    if exist "C:\Qt\6.4.0\msvc2019_64" set QT_DIR=C:\Qt\6.4.0\msvc2019_64
    if exist "C:\Qt\5.15.2\msvc2019_64" set QT_DIR=C:\Qt\5.15.2\msvc2019_64
    if exist "C:\Qt\5.15.2\mingw81_64" set QT_DIR=C:\Qt\5.15.2\mingw81_64
)

if "%QT_DIR%"=="" (
    echo WARNING: Qt not found in common paths. Set QT_DIR or CMAKE_PREFIX_PATH.
    echo Example: set QT_DIR=C:\Qt\5.15.2\msvc2019_64
    echo.
)

REM Create build directory
if not exist build mkdir build
cd build

REM Configure
if "%QT_DIR%"=="" (
    cmake .. -DCMAKE_BUILD_TYPE=Release
) else (
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_DIR%"
)

if errorlevel 1 (
    echo.
    echo CMake configuration failed. Ensure Qt and CMake are installed.
    echo For Qt: https://www.qt.io/download
    echo Set QT_DIR to your Qt kit path, e.g. C:\Qt\5.15.2\msvc2019_64
    cd ..
    exit /b 1
)

REM Build
cmake --build . --config Release -j %NUMBER_OF_PROCESSORS%

if errorlevel 1 (
    echo Build failed.
    cd ..
    exit /b 1
)

cd ..
echo.
echo Build completed!
echo Run: build\Release\DBCViewer.exe  (or build\DBCViewer.exe for Ninja/MinGW)
echo Or use: run_dbc_viewer.bat
echo.
