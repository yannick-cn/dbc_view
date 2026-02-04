@echo off
REM DBC Viewer Windows Launch Script

if exist "build\Release\DBCViewer.exe" (
    echo Starting DBC Viewer...
    start "" "build\Release\DBCViewer.exe"
    exit /b 0
)

if exist "build\DBCViewer.exe" (
    echo Starting DBC Viewer...
    start "" "build\DBCViewer.exe"
    exit /b 0
)

echo Error: DBCViewer.exe not found.
echo Please run build.bat first to compile the application.
exit /b 1
