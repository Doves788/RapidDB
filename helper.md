# RapidDB Architecture & Testing Guide

This document explains the core concepts, architecture, supported commands, and testing procedures used in RapidDB.

---

# System Architecture

## Core Concepts

| Concept                  | Description                                                            |
| ------------------------ | ---------------------------------------------------------------------- |
| TCP Socket Programming   | Enables communication between clients and the server over the network. |
| Multithreading           | Allows multiple clients to interact with the database simultaneously.  |
| Mutex Synchronization    | Protects shared resources from race conditions.                        |
| Hash Tables              | Used for efficient key-value lookups.                                  |
| Dynamic Arrays (Vectors) | Used to implement list data structures.                                |
| RESP Protocol            | Parses Redis-compatible client requests and responses.                 |
| Persistence              | Stores in-memory data on disk and restores it on startup.              |
| Signal Handling          | Ensures graceful shutdown and final data synchronization.              |
| Singleton Pattern        | Maintains a single shared database instance.                           |
| STL Containers           | Provides efficient storage and utility operations.                     |

---

## Class Responsibilities

### Server

Responsible for:

* Socket creation
* Port binding
* Connection acceptance
* Client thread management
* Graceful shutdown handling

### Database

Responsible for:

* String storage
* List storage
* Hash storage
* Expiration management
* Persistence operations
* Thread-safe data access

### Command Handler

Responsible for:

* RESP parsing
* Command validation
* Command routing
* Response generation

---

# Data Structures

### String Store

```cpp
unordered_map<string, string>
```

Stores standard key-value pairs.

Example:

```text
user:name -> Alice
```

---

### List Store

```cpp
unordered_map<string, vector<string>>
```

Stores ordered collections.

Example:

```text
tasks -> [task1, task2, task3]
```

---

### Hash Store

```cpp
unordered_map<string,
    unordered_map<string, string>>
```

Stores structured field-value mappings.

Example:

```text
profile:1
├── name -> Alice
├── age -> 21
└── city -> Delhi
```

---

# Persistence System

RapidDB periodically saves data to:

```text
dump.rapiddb
```

Persistence workflow:

```text
Memory
   │
   ▼
dump()
   │
   ▼
dump.rapiddb
   │
   ▼
load()
   │
   ▼
Memory
```

Features:

* Automatic snapshot every 300 seconds
* Startup recovery
* Graceful shutdown synchronization

---

# Expiration System

The `EXPIRE` command associates a TTL with a key.

Example:

```text
SET session token
EXPIRE session 60
```

After 60 seconds the key is automatically removed.

Expired entries are cleaned through lazy expiration during data access operations.

---

# Supported Commands

## Common Commands

| Command  | Description              |
| -------- | ------------------------ |
| PING     | Returns PONG             |
| ECHO     | Returns supplied message |
| FLUSHALL | Removes all stored data  |

---

## String Commands

| Command | Description      |
| ------- | ---------------- |
| SET     | Store value      |
| GET     | Retrieve value   |
| KEYS    | List all keys    |
| TYPE    | Return data type |
| DEL     | Delete key       |
| UNLINK  | Delete key       |
| EXPIRE  | Set TTL          |
| RENAME  | Rename key       |

---

## List Commands

| Command | Description              |
| ------- | ------------------------ |
| LPUSH   | Insert at front          |
| RPUSH   | Insert at back           |
| LPOP    | Remove from front        |
| RPOP    | Remove from back         |
| LLEN    | Return list length       |
| LGET    | Return all elements      |
| LINDEX  | Access by index          |
| LSET    | Update by index          |
| LREM    | Remove matching elements |

---

## Hash Commands

| Command | Description                  |
| ------- | ---------------------------- |
| HSET    | Set field                    |
| HGET    | Get field                    |
| HEXISTS | Check field existence        |
| HDEL    | Delete field                 |
| HLEN    | Count fields                 |
| HKEYS   | Return fields                |
| HVALS   | Return values                |
| HGETALL | Return all fields and values |
| HMSET   | Set multiple fields          |

---

# Manual Testing

## Start Server

```bash
./RapidDB
```

Custom port:

```bash
./RapidDB 6380
```

---

## Connect Using Redis CLI

```bash
redis-cli -p 6379
```

---

## Basic Verification

### Connectivity

```text
PING
```

Expected:

```text
PONG
```

---

### String Operations

```text
SET username Alice
GET username
TYPE username
```

Expected:

```text
OK
Alice
string
```

---

### List Operations

```text
RPUSH tasks task1 task2 task3
LLEN tasks
LGET tasks
```

Expected:

```text
3
task1
task2
task3
```

---

### Hash Operations

```text
HSET user name Alice
HSET user age 21

HGET user name
HGETALL user
```

Expected:

```text
Alice

name
Alice
age
21
```

---

# Future Enhancements

* Sorted Sets
* Transactions
* Publish/Subscribe
* Append Only File (AOF)
* Replication
* Authentication
* Cluster Support
