# Multi-stage / Production Dockerfile for PakSaver Multi-DSA C++ Platform
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV PORT=8080
ENV PYTHONUNBUFFERED=1
ENV TZ=Asia/Karachi

# Install C++ compiler, Python, and networking tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    g++ \
    python3 \
    python3-pip \
    tzdata \
    curl \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /app

# Copy application files
COPY httplib.h server.cpp scraper.py live_scraper.py ./
COPY public ./public

# Compile C++ Multi-DSA HTTP Server with optimizations
RUN g++ -std=c++17 -O3 server.cpp -o server -lpthread

# Run initial 100% live bank scraper to populate deals_data.csv
RUN python3 scraper.py || true

# Healthcheck for cloud container orchestrators
HEALTHCHECK --interval=30s --timeout=5s --start-period=5s --retries=3 \
  CMD curl -f http://localhost:${PORT}/health || exit 1

EXPOSE 8080

CMD ["./server"]
