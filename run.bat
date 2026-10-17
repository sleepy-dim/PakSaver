@echo off
title PakSaver - Pakistani Bank Discounts Platform
echo ======================================================================
echo  PakSaver Multi-DSA Platform Launcher (Windows)
echo ======================================================================

REM Check if server.exe exists
if not exist server.exe (
    echo [*] Compiling C++ server...
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    cl.exe /std:c++17 /EHsc /O2 server.cpp /Fe:server.exe ws2_32.lib crypt32.lib
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
