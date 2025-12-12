FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libssl-dev \
    pkg-config \
    git \
    libboost-all-dev

WORKDIR /app
COPY . .

RUN mkdir build && cd build && \
    cmake .. && \
    make

FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libssl3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/gateway_service .

EXPOSE 8080
CMD ["./gateway_service"]