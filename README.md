# PakSaver 🇵🇰 — Nationwide Multi-DSA C++ Bank Discount Aggregator

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Python 3.10+](https://img.shields.io/badge/Python-3.10+-yellow.svg)](https://www.python.org/)
[![Docker](https://img.shields.io/badge/Docker-Ready-2496ED.svg)](https://www.docker.com/)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

**PakSaver** is a high-performance, real-world Bank & Digital Wallet discount aggregation platform for Pakistan. It combines an asynchronous **Python web scraper** that ingests real-time bank deals nationwide with a **100% custom C++ Backend** powered by manual Data Structures & Algorithms (DSA)—zero database dependencies.

---

## 🚀 Key Highlights & Dataset

- **85,000+ Live Nationwide Deals**: Covering all 33 major Pakistani cities (Karachi, Lahore, Islamabad, Rawalpindi, Faisalabad, Multan, Peshawar, Quetta, Sialkot, Gujranwala, Hyderabad, and more).
- **All Commercial & Islamic Banks**: Real discount tiers from Meezan Bank, HBL, Bank Alfalah, Allied Bank, Askari Bank, Faysal Bank, UBL, Bank of Punjab, MCB, Bank AL Habib, BankIslami, Standard Chartered, JS Bank, Soneri Bank, and digital wallets (Easypaisa, PayPak, etc.).
- **Zero Hardcoded Values**: Auto-discovers every restaurant, bakery, clinic, retail merchant, qualifying card tier, discount percentage, and cap limit. Non-banks (utility, insurance, telco) are strictly filtered out.
- **Microsecond C++ In-Memory Engine**: Hand-crafted manual data structures delivering sub-millisecond query execution.

---

## 🧠 Data Structures & Algorithms (DSA) Architecture

| Data Structure / Algorithm | Role in PakSaver | Time Complexity |
| :--- | :--- | :--- |
| **CityHashTable** *(Separate Chaining)* | Maps city names directly to linked buckets of restaurants via Polynomial Rolling Hash (`h = (h * 31 + c) % TABLE_SIZE`). | **$O(1)$ Average** |
| **CardDealNode List** *(Singly Linked List)* | Each restaurant node maintains a linked list of qualifying card products, caps, min spend rules, and terms. | **$O(K)$** per merchant |
| **DiscountBST** *(Binary Search Tree)* | Dynamically built on filtered queries to sort restaurants from highest discount to lowest via Reverse In-Order Traversal. Prunes subtrees when minimum discount thresholds are applied. | **$O(\log N)$ Insert / Traversal with Range Pruning** |
| **Sorted Directory** *(Pointer Array)* | Alphabetically indexed directory of all restaurants sorted with $O(N \log N)$ quicksort during CSV load. | **$O(\log N)$ Iterative Binary Search** |
| **RecentSearchQueue** *(Circular Array Queue)* | Fixed-capacity queue recording real-time search queries and telemetry. | **$O(1)$ Enqueue / Dequeue** |

---

## 🛠️ Project Structure

```text
PakSaver/
├── server.cpp            # Custom C++ Backend (HashTable, Linked List, BST, Binary Search, HTTP server)
├── httplib.h             # Lightweight C++ single-header HTTP server
├── scraper.py            # High-speed parallel Python scraper (ThreadPool, multi-city branch expansion)
├── live_scraper.py       # Live scraper hook invoked from UI via POST /api/scrape-live
├── deals_data.csv        # Generated dataset containing 85,000+ verified deals
├── public/
│   └── index.html        # Glassmorphic frontend with interactive bill calculator & DSA telemetry
├── Dockerfile            # Multi-stage production container definition
├── docker-compose.yml    # 1-click Docker orchestration
├── render.yaml           # 1-click cloud deployment specification
├── run.bat               # 1-click Windows launcher
└── start.sh              # 1-click Linux / macOS launcher
```

---

## ⚡ Quick Start

### Option 1: Native Windows (Pre-requisites: MSVC or g++, Python 3)
```powershell
# 1. Scrape live bank deals
python scraper.py

# 2. Compile and run C++ server
run.bat
```
Visit `http://localhost:8080/` in your browser.

### Option 2: Docker (Recommended for complete deployment)
```bash
docker compose up --build
```

---

## 🌐 Cloud Deployment

PakSaver is cloud-ready and runs on any container platform (Render, Railway, Fly.io, AWS, Heroku):
- Dynamic `$PORT` binding support
- Built-in `/health` endpoint for orchestrator health checks
- Multi-stage Docker build

---

## 📄 License
This project is open-source under the [MIT License](LICENSE).
