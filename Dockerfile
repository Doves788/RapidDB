# ==========================================
# RapidDB Multi-Stage Production Dockerfile
# ==========================================

# Stage 1: Build Environment
FROM alpine:3.20 AS builder

RUN apk add --no-cache \
    g++ \
    make \
    linux-headers

WORKDIR /build

COPY Makefile ./
COPY include/ ./include/
COPY src/ ./src/

# Compile optimized release binary
RUN make clean && make -j$(nproc)

# Stage 2: Lightweight Runtime Environment
FROM alpine:3.20 AS runtime

RUN apk add --no-cache \
    libstdc++ \
    libgcc

WORKDIR /app

# Copy binary from builder
COPY --from=builder /build/RapidDB /app/RapidDB

# Persistent data directory
VOLUME ["/data"]
WORKDIR /data

# Expose default Redis/RapidDB port
EXPOSE 6379

ENTRYPOINT ["/app/RapidDB"]
CMD ["6379"]

