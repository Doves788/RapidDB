# RapidDB

> A lightweight **Redis-compatible in-memory database** built from scratch in **C++17** — implementing the RESP wire protocol, concurrent TCP networking, TTL-based expiration, and crash-safe disk persistence. Compatible with the standard `redis-cli`, so anything that speaks RESP can talk to it.

---

## ⚙️ Engineering Highlights

A quick scan of what's non-trivial under the hood:

- **RESP protocol parser** — parses inline and multi-bulk commands directly off a raw TCP stream, with malformed-input handling
- **Dispatch-table command routing** — commands are routed via a `std::unordered_map<string, handler>` lookup instead of an if-else chain, making the codebase O(1) per dispatch and easy to extend with new commands
- **Thread-per-connection concurrency** — each client gets a dedicated `std::thread`; shared data structures are protected with mutexes
- **TTL & expiration system** — lazy expiration on access plus periodic active cleanup, avoiding unbounded memory growth from stale keys
- **Snapshot persistence engine** — serializes the entire keyspace to disk on a timer and on graceful shutdown, with automatic recovery on startup
- **Three data types** (Strings, Lists, Hashes) backed by STL containers behind a Singleton-managed store

---

## 📑 Table of Contents

- [Features](#-features)
- [Architecture](#-architecture)
- [Design Decisions & Trade-offs](#-design-decisions--trade-offs)
- [Supported Commands](#-supported-commands)
- [Tech Stack](#️-tech-stack)
- [Project Structure](#-project-structure)
- [Getting Started](#️-getting-started)
- [Persistence](#-persistence)
- [Usage Examples](#-usage-examples)
- [Benchmarking](#-benchmarking)
- [License](#-license)

---

## ✨ Features

- Redis-inspired in-memory key-value store
- Full RESP (Redis Serialization Protocol) support — works with the real `redis-cli`
- Multi-client concurrent connections via threads
- Thread-safe operations via mutex synchronization
- Supports **Strings**, **Lists**, and **Hashes**
- Key expiration with TTL (`EXPIRE`)
- Automatic snapshot persistence + startup recovery
- Graceful shutdown with guaranteed persistence

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
                         │
                Command Dispatcher
                 (hash-map lookup)
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

**Storage engine** (all behind a Singleton-managed database instance):

| Type    | Underlying structure                              |
| ------- | -------------------------------------------------- |
| Strings | `unordered_map<string, string>`                     |
| Lists   | `unordered_map<string, vector<string>>`             |
| Hashes  | `unordered_map<string, unordered_map<string, string>>` |

---

## 🧩 Design Decisions & Trade-offs

Being able to talk through *why*, not just *what*, is what this section is for — these are the trade-offs I'd bring up in an interview:

- **Thread-per-connection vs. thread pool / event loop:** Simple to reason about and implement correctly, but doesn't scale past a few hundred concurrent clients due to thread creation/context-switch overhead. A production version would move to an event loop (epoll) or a bounded thread pool.
- **Single global mutex vs. fine-grained/sharded locking:** The current design locks around shared state per command, which is correctness-first but caps throughput under high concurrency since all writes serialize. Sharding the keyspace (e.g., by hashing keys into N independently-locked partitions, the way real Redis-like systems do) is the natural next step.
- **Snapshotting vs. AOF (Append-Only File):** Snapshots are simple and fast to implement but lose all writes since the last snapshot on a crash. An AOF would give stronger durability at the cost of write throughput — a classic durability vs. performance trade-off.
- **Singleton database instance:** Keeps access simple and avoids passing state around, but makes unit testing harder and rules out running multiple isolated instances in one process. A dependency-injected store would be more testable.

---

## 📚 Supported Commands

**Strings**

| Command | Description         |
| ------- | -------------------- |
| SET     | Store a value        |
| GET     | Retrieve a value     |
| KEYS    | List all keys        |
| TYPE    | Get data type        |
| DEL     | Delete key            |
| UNLINK  | Non-blocking delete   |
| EXPIRE  | Set key expiration    |
| RENAME  | Rename key            |

**Lists**

| Command | Description           |
| ------- | ----------------------- |
| LPUSH   | Insert at front          |
| RPUSH   | Insert at back           |
| LPOP    | Remove first element     |
| RPOP    | Remove last element      |
| LGET    | Get complete list        |
| LLEN    | List length               |
| LINDEX  | Get element by index     |
| LSET    | Modify element           |
| LREM    | Remove element           |

**Hashes**

| Command  | Description          |
| -------- | ---------------------- |
| HSET     | Set field              |
| HGET     | Get field              |
| HEXISTS  | Check field            |
| HDEL     | Delete field           |
| HLEN     | Number of fields       |
| HKEYS    | Get all keys           |
| HVALS    | Get all values         |
| HGETALL  | Get complete hash      |
| HMSET    | Set multiple fields    |

**Common**

`PING` · `ECHO` · `FLUSHALL`

---

## 🛠️ Tech Stack

C++17 · POSIX TCP Sockets · `std::thread` · Mutex synchronization · RESP protocol · STL containers · Makefile

---

## 📂 Project Structure

```text
RapidDB
│
├── include/
│   ├── Command_Handler.h
│   ├── Database.h
│   └── Server.h
│
├── src/
│   ├── Command_Handler.cpp
│   ├── Database.cpp
│   ├── Server.cpp
│   └── main.cpp
│
├── helper.md
├── Makefile
├── LICENSE
└── README.md
```

---

## ⚙️ Getting Started

**Build**

```bash
make
```

**Clean**

```bash
make clean
```

**Manual compile (no Make)**

```bash
g++ -std=c++17 -pthread -Iinclude src/*.cpp -o RapidDB
```

**Run**

```bash
./RapidDB          # default port 6379
./RapidDB 6380     # custom port
```

**Connect with the real Redis CLI**

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

## 💾 Persistence

- Snapshots auto-saved to `dump.rapiddb` every **300 seconds**
- Full keyspace serialized/deserialized on save and load
- Automatic recovery from the last snapshot on startup
- Guaranteed flush-to-disk on graceful shutdown (SIGINT/SIGTERM)

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

## 📊 Benchmarking

No throughput numbers are published here yet — rather than guess, run it yourself with the standard Redis benchmarking tool against the server:

```bash
redis-benchmark -p 6379 -t set,get -n 100000 -c 50
```

If you add real numbers, this is also a good place to note the current bottleneck candidly: the global-lock design above means throughput will plateau as concurrency (`-c`) increases — which is exactly the kind of honest, specific detail that holds up well under interview follow-up questions.

---

## 📖 Further Reading

Implementation notes and extended examples live in [`helper.md`](helper.md).

---

## 📄 License

Licensed under the **Boost Software License 1.0**. See [LICENSE](LICENSE) for details.

---
