# RapidDB
A lightweight Redis-compatible in-memory database built in C++. RapidDB supports strings, lists, and hashes, implements the Redis Serialization Protocol (RESP), handles multiple concurrent clients, and provides periodic disk persistence.

---
## Features

* Redis-inspired in-memory key-value database
* Full RESP (Redis Serialization Protocol) support
* Multi-client concurrent connections using threads
* Thread-safe data access using mutex synchronization
* Support for String, List, and Hash data structures
* Key expiration with TTL support
* Automatic persistence through periodic snapshots
* Graceful shutdown with data persistence
* Startup recovery from saved snapshots

---

## Supported Data Types

### Strings

* SET
* GET
* KEYS
* TYPE
* DEL / UNLINK
* EXPIRE
* RENAME

### Lists

* LPUSH
* RPUSH
* LPOP
* RPOP
* LGET
* LLEN
* LINDEX
* LSET
* LREM

### Hashes

* HSET
* HGET
* HEXISTS
* HDEL
* HLEN
* HKEYS
* HVALS
* HGETALL
* HMSET

### Common Commands

* PING
* ECHO
* FLUSHALL

---

## Project Structure

```text
RapidDB
├── include/
│   ├── Command_Handler.h
│   ├── Database.h
│   └── Server.h
├── src/
│   ├── Command_Handler.cpp
│   ├── Database.cpp
│   ├── Server.cpp
│   └── main.cpp
├── helper.md
├── Makefile
├── README.md
```

---

## Build Instructions

### Using Makefile

```bash
make
```

### Clean Build Files

```bash
make clean
```

### Manual Compilation

```bash
g++ -std=c++17 -pthread -Iinclude src/*.cpp -o RapidDB
```

---

## Running the Server

Start the server on the default port:

```bash
./RapidDB
```

Start the server on a custom port:

```bash
./RapidDB 6380
```

Default port: `6379`

---

## Persistence

RapidDB automatically saves the in-memory dataset to:

```text
dump.rapiddb
```

Features:

* Automatic snapshot every 300 seconds
* Recovery from disk on startup
* Graceful shutdown persistence using signal handling

---

## Connecting with Redis CLI

You can interact with RapidDB using the standard Redis client:

```bash
redis-cli -p 6379
```

Example:

```text
127.0.0.1:6379> PING
PONG

127.0.0.1:6379> SET name Alice
OK

127.0.0.1:6379> GET name
Alice
```

---

## Command Examples

### String Operations

```text
SET user Alice
GET user
TYPE user
DEL user
```

### List Operations

```text
LPUSH fruits apple banana orange
LGET fruits
LLEN fruits
LPOP fruits
```

### Hash Operations

```text
HSET user name Alice
HSET user age 21

HGET user name
HGETALL user
```

---

## Architecture

### Networking

* TCP socket-based server
* Redis-compatible request handling
* RESP protocol parsing

### Concurrency

* Thread-per-client architecture
* Independent client handling
* Mutex-protected shared database

### Storage Engine

#### String Store

```cpp
unordered_map<string, string>
```

#### List Store

```cpp
unordered_map<string, vector<string>>
```

#### Hash Store

```cpp
unordered_map<string, unordered_map<string, string>>
```

### Expiration System

* TTL support via `EXPIRE`
* Lazy expiration through periodic cleanup
* Automatic removal of expired keys

### Persistence Engine

* Snapshot-based storage
* File serialization and deserialization
* Dataset restoration during startup

### Design Pattern

RapidDB uses the Singleton Pattern to maintain a single shared database instance throughout the application.

---

## Testing

Run the provided test suite:

```bash
./test_all.sh
```

Additional command examples and implementation notes can be found in:

```text
helper.md
```

---

## Future Improvements

* Transactions
* Publish/Subscribe support
* Sorted Sets
* AOF (Append Only File) persistence
* Replication support
* Cluster mode
* Authentication and access control

---