@echo off
echo Building Smart PC Controller...

:: Check if Visual Studio tools are available
where cl >nul 2>nul
if %errorlevel% neq 0 (
    echo Visual Studio compiler not found. Please run this from a Visual Studio Developer Command Prompt.
    echo Or run: "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    pause
    exit /b 1
)

:: Compile the program
cl /EHsc /O2 /MT smart_pc_controller.cpp user32.lib advapi32.lib ws2_32.lib shell32.lib comctl32.lib /Fe:SmartPCController.exe

if %errorlevel% equ 0 (
    echo.
    echo Build successful! Created SmartPCController.exe
    echo.
    echo To run the program, execute: SmartPCController.exe
    echo The program will run in the background with a system tray icon.
) else (
    echo.
    echo Build failed! Please check the error messages above.
)

pause

