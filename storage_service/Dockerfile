FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libpqxx-dev \
    libboost-all-dev \
    libssl-dev \
    pkg-config \
    git

WORKDIR /app
COPY . .

RUN mkdir build && cd build && \
    cmake .. && \
    make

FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libpqxx-6.4 \
    libpq5 \
    libssl3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/storage_service .
RUN mkdir uploads
EXPOSE 8080
CMD ["./storage_service"]