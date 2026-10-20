# RapidDB

> A lightweight, highly reliable **Redis-compatible in-memory database** built from scratch in **C++17** — implementing the RESP wire protocol, multi-threaded networking, TTL-based expiration, and crash-safe disk persistence. Compatible with standard `redis-cli`, containerized via Docker, and verified with AddressSanitizer (ASan) and a dedicated Chaos Testing suite.

[![CI Quality Gate](https://github.com/Doves788/RapidDB/actions/workflows/ci.yml/badge.svg)](https://github.com/Doves788/RapidDB/actions/workflows/ci.yml)
![Language](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg)
![Sanitizer](https://img.shields.io/badge/Sanitizer-AddressSanitizer%20%7C%20UBSan-brightgreen.svg)
![Docker](https://img.shields.io/badge/Docker-Ready-2496ED.svg)
![License](https://img.shields.io/badge/License-Boost%201.0-orange.svg)

---

## ⚙️ Engineering Highlights

A quick scan of what's non-trivial under the hood:

- **Defensive RESP Protocol Parser** — parses inline commands and multi-bulk arrays directly from raw TCP streams with bounds checking, exception-safe integer parsing, and graceful handling of malformed or desynchronized frames.
- **Zero Memory Leaks Under Concurrency** — profiled and validated under GCC AddressSanitizer (`-fsanitize=address,undefined`) and LeakSanitizer across 10,000 concurrent key insertions and lazy TTL evictions.
- **Thread-Per-Connection Concurrency** — handles multi-client connections via dedicated threads with safe socket lifecycle management and `SIGPIPE` immunity.
- **Lazy TTL & Expiration Subsystem** — steady-clock expiration tracking with lazy eviction across Strings, Lists, and Hashes to prevent unbounded memory growth.
- **Crash-Safe Persistence** — snapshot persistence engine auto-dumping to `dump.rapiddb` on a 300-second timer and guaranteed flush-on-exit via `SIGINT`/`SIGTERM` signal handlers.
- **Chaos Testing & Fuzzing Suite** — verified against slowloris-style 1-byte packet fragmentation, 100-socket burst disconnects, 1MB bulk payloads, and random byte fuzzing.
- **Production Shipping (Docker & CI)** — packaged as a multi-stage Alpine Docker image and automated via GitHub Actions CI pipelines with ASan verification gates.

---

## 📑 Table of Contents

- [Features](#-features)
- [Architecture](#-architecture)
- [Design Decisions & Trade-offs](#-design-decisions--trade-offs)
- [Reliability & Testing Suite](#-reliability--testing-suite)
- [Supported Commands](#-supported-commands)
- [Tech Stack](#️-tech-stack)
- [Project Structure](#-project-structure)
- [Getting Started](#️-getting-started)
- [Docker & Container Deployment](#-docker--container-deployment)
- [Persistence](#-persistence)
- [Usage Examples](#-usage-examples)
- [Roadmap & Future Enhancements](#-roadmap--future-enhancements)
- [License](#-license)

---

## ✨ Features

- Redis-inspired in-memory key-value store
- Full RESP (Redis Serialization Protocol) support — works with the real `redis-cli`
- Multi-client concurrent connections via threads
- Thread-safe operations via mutex synchronization
- Supports **Strings**, **Lists**, and **Hashes**
- Key expiration with TTL (`EXPIRE`) and lazy eviction
- Automatic snapshot persistence + startup recovery
- Immune to `SIGPIPE` abruptly disconnected clients
- Automated test suites (Chaos reliability & Memory leak verification)

---

## 🏗️ Architecture

```text
       Client 1    Client 2    Client 3
           │            │           │
           └────────────┼───────────┘
                         │
               TCP Socket Server
             (1 thread per client)
                         │
                RESP Command Parser
            (stream buffer accumulator)
                         │
                Command Dispatcher
                         │
        ┌────────────────┼────────────────┐
        │                │                │
    Strings           Lists            Hashes
        │                │                │
        └────────────────┼────────────────┘
                         │
                Persistence Engine
                         │
                   dump.rapiddb
```

**Storage Engine** (Singleton-managed database store):

| Type    | Underlying Structure                                |
| ------- | --------------------------------------------------- |
| Strings | `std::unordered_map<string, string>`                |
| Lists   | `std::unordered_map<string, vector<string>>`        |
| Hashes  | `std::unordered_map<string, unordered_map<string, string>>` |

---

## 🧩 Design Decisions & Trade-offs

- **Thread-per-connection vs. Event Loop (epoll):** Simple to reason about and implement cleanly. However, thread context switches cap scale at hundreds of concurrent clients. A production evolution would transition to an event loop (`epoll` / `io_uring`) or bounded thread pool.
- **Global mutex vs. Fine-grained / Sharded locking:** The current design synchronizes around shared state per operation via `std::lock_guard`, which guarantees safety first. Sharding the keyspace into partitioned buckets with dedicated mutexes is the next logical step.
- **Snapshotting vs. AOF (Append-Only File):** Snapshots write periodic state to `dump.rapiddb`. While fast and simple, unwritten mutations between snapshots are vulnerable to ungraceful crashes. An Append-Only File (AOF) offers higher durability at the trade-off of append overhead.
- **Lazy Eviction vs. Active Background Sweeping:** Lazy eviction purges expired keys on-demand whenever a key or collection is touched. This keeps read/write paths overhead-minimal while avoiding locks for background workers.

---

## 🛡️ Reliability & Testing Suite

Rather than relying purely on synthetic benchmarks, RapidDB is verified for **memory safety**, **protocol reliability**, and **resilience under chaos**:

### 1. Memory Safety & Leak Verification (`tests/memory_leak_test.py`)
- Built with GCC **AddressSanitizer (ASan)** and **UndefinedBehaviorSanitizer (UBSan)**.
- Stresses the database with **10,000 keys** and associated TTL expiration timers.
- Validates that upon expiration, all 10,000 keys are completely evicted from memory (`KEYS` returns `*0`), `DEL` return codes are accurate (`:1` for success, `:0` for miss), and zero memory or thread leaks occur.

Run the memory leak test:
```bash
python3 tests/memory_leak_test.py
```

### 2. Protocol Chaos & Fuzzing Suite (`tests/chaos_test.py`)
Validates that RapidDB never crashes when subjected to hostile or malformed traffic:
- **Malformed multi-bulk arrays:** `*abc\r\n`, `*-5\r\n`, `*99999999999999999999\r\n`
- **Malformed bulk strings:** non-numeric lengths, truncated data, negative lengths
- **Slowloris-style fragmentation:** commands streamed 1 byte at a time with delays
- **Rapid connection hammering:** 100 concurrent sockets opened and instantly aborted
- **Large payloads:** 1MB string `SET` and `GET` roundtrip verification
- **Random byte fuzzing:** arbitrary random byte sequences to verify the parser discards invalid packets without crashing

Run the chaos test suite:
```bash
python3 tests/chaos_test.py
```

Or run all automated test suites at once:
```bash
make test
```

---

## 📚 Supported Commands

**Strings & Keys**

| Command | Description         |
| ------- | ------------------- |
| SET     | Store a value       |
| GET     | Retrieve a value    |
| KEYS    | List all keys       |
| TYPE    | Get data type       |
| DEL     | Delete key          |
| UNLINK  | Delete key          |
| EXPIRE  | Set key expiration  |
| RENAME  | Rename key          |

**Lists**

| Command | Description          |
| ------- | -------------------- |
| LPUSH   | Insert at front      |
| RPUSH   | Insert at back       |
| LPOP    | Remove first element |
| RPOP    | Remove last element  |
| LGET    | Get complete list    |
| LLEN    | List length          |
| LINDEX  | Get element by index |
| LSET    | Modify element       |
| LREM    | Remove element       |

**Hashes**

| Command  | Description          |
| -------- | -------------------- |
| HSET     | Set field            |
| HGET     | Get field            |
| HEXISTS  | Check field          |
| HDEL     | Delete field         |
| HLEN     | Number of fields     |
| HKEYS    | Get all keys         |
| HVALS    | Get all values       |
| HGETALL  | Get complete hash    |
| HMSET    | Set multiple fields  |

**Common**

`PING` · `ECHO` · `FLUSHALL`

---

## 🛠️ Tech Stack

C++17 · POSIX TCP Sockets · `std::thread` · Mutex synchronization · AddressSanitizer (ASan) · Docker · GitHub Actions CI · Makefile · Python 3

---

## 📂 Project Structure

```text
RapidDB
│
├── .github/
│   └── workflows/
│       └── ci.yml             # GitHub Actions CI matrix & ASan quality gate
├── include/
│   ├── Command_Handler.h      # RESP parser & command dispatcher declarations
│   ├── Database.h             # Storage engine, TTL eviction, persistence
│   └── Server.h               # TCP socket listener & connection thread management
├── src/
│   ├── Command_Handler.cpp    # Stream buffer parsing & command execution
│   ├── Database.cpp           # In-memory stores, purgeExpired, dump & load
│   ├── Server.cpp             # Connection loop, buffer accumulator, signal handlers
│   └── main.cpp               # Server entry point & periodic snapshot timer
├── tests/
│   ├── chaos_test.py          # Protocol fuzzing & malformed packet testing
│   └── memory_leak_test.py    # 10,000 key TTL stress & memory leak verification
├── Dockerfile                 # Multi-stage lightweight Alpine build
├── docker-compose.yml         # Container compose definition
├── .dockerignore
├── Makefile                   # Build rules (release, asan, test, clean)
├── helper.md                  # Extended architecture and manual testing guide
├── LICENSE
└── README.md
```

---

## ⚙️ Getting Started

### Standard Build (Release)
```bash
make
```

### Build with AddressSanitizer (ASan) & UBSan
```bash
make asan
```

### Run Tests
```bash
make test
```

### Run Server Locally
```bash
./RapidDB          # default port 6379
./RapidDB 6380     # custom port
```

### Connect with standard Redis CLI
```bash
redis-cli -p 6379
```

```text
127.0.0.1:6379> PING
PONG
127.0.0.1:6379> SET name Alice
OK
127.0.0.1:6379> GET name
Alice
```

---

## 🐳 Docker & Container Deployment

RapidDB is fully containerized using a multi-stage Alpine build for minimal image size (~15MB).

### Build & Run via Docker Compose
```bash
docker compose up -d
```

### Build & Run via Docker CLI
```bash
# Build image
docker build -t rapiddb .

# Run container with persistent storage
docker run -d -p 6379:6379 -v rapiddb_data:/data --name rapiddb rapiddb:latest
```

---

## 💾 Persistence

- Snapshots auto-saved to `dump.rapiddb` every **300 seconds**
- Full keyspace serialized/deserialized on save and load
- Automatic recovery from the last snapshot on startup
- Guaranteed flush-to-disk on graceful shutdown (`SIGINT` / `SIGTERM`)

---

## 💻 Usage Examples

```text
# Strings
SET user Alice
GET user
TYPE user
DEL user

# Lists
LPUSH fruits apple banana orange
LGET fruits
LLEN fruits
LPOP fruits

# Hashes
HSET user name Alice
HSET user age 21
HGETALL user
```

---

## 🚀 Roadmap & Future Enhancements

- [ ] **I/O Multiplexing (`epoll` / `io_uring`):** Transition from thread-per-connection to an event-driven non-blocking I/O loop to scale to 100,000+ idle concurrent connections.
- [ ] **Fine-Grained Keyspace Partitioning:** Shard the internal database hash maps into $N$ buckets with independent mutex locks to eliminate write serialization under heavy load.
- [ ] **Append-Only File (AOF) Persistence:** Provide write-ahead logging with `fsync` policies (`always`, `everysec`, `no`) for enterprise durability.
- [ ] **Sorted Sets (`ZSET`):** Implement skip-lists or balanced trees for ordered rank and range queries.
- [ ] **Transactions (`MULTI` / `EXEC`):** Provide atomic batch command execution.
- [ ] **Pub/Sub Subsystem:** Message broking with channels and pattern subscriptions.

---

## 📄 License

Licensed under the **Boost Software License 1.0**. See [LICENSE](LICENSE) for details.
