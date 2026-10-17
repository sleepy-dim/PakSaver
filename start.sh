#!/usr/bin/env bash
set -e

echo "======================================================================"
echo " PakSaver Multi-DSA Platform Launcher (Linux / macOS)"
echo "======================================================================"

PORT="${PORT:-8080}"

if [ ! -f ./server ]; then
    echo "[*] Compiling server.cpp with g++ (C++17)..."
    g++ -std=c++17 -O3 server.cpp -o server -lpthread
fi

if [ ! -f ./deals_data.csv ]; then
    echo "[*] Scraping live deals across Pakistan..."
    python3 scraper.py || python scraper.py
fi

echo "[*] Starting server on http://0.0.0.0:${PORT} ..."
./server
