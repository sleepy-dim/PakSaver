@echo off
setlocal EnableExtensions EnableDelayedExpansion
title PakSaver - Pakistani Bank Discounts Platform
echo ======================================================================
echo  PakSaver Multi-DSA Platform Launcher (Windows)
echo ======================================================================

REM Check if server.exe exists
if not exist server.exe (
    where g++ >nul 2>nul
    if not errorlevel 1 (
        echo [*] Compiling C++ server with g++...
        g++ -std=c++17 -O2 server.cpp -o server.exe -lws2_32 -lcrypt32
    ) else (
        set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
        if exist "!VSWHERE!" (
            for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%i"
        )
        if defined VS_PATH if exist "!VS_PATH!\VC\Auxiliary\Build\vcvars64.bat" (
            call "!VS_PATH!\VC\Auxiliary\Build\vcvars64.bat"
        )
        where cl.exe >nul 2>nul
        if not errorlevel 1 (
            echo [*] Compiling C++ server with MSVC...
            cl.exe /std:c++17 /EHsc /O2 server.cpp /Fe:server.exe ws2_32.lib crypt32.lib
        ) else (
            echo [ERROR] Install MSVC Build Tools or MinGW-w64 g++, then run this launcher again.
            exit /b 1
        )
    )
    if errorlevel 1 exit /b 1
)

REM Check if deals_data.csv exists
if not exist deals_data.csv (
    echo [*] Pulling live nationwide banking deals...
    python scraper.py
)

echo [*] Starting PakSaver C++ Multi-DSA Backend on http://localhost:8080 ...
start http://localhost:8080
server.exe
pause
