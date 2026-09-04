# MiniDB

<div align="center">

# 🗄️ MiniDB

### A Small, Concurrent & Persistent Key-Value Database Written in C

**TCP • Multithreading • WAL • Snapshots • Expiration • Crash Recovery**

<br>

![C11](https://img.shields.io/badge/C-C11-blue?style=for-the-badge)
![CMake](https://img.shields.io/badge/CMake-Build-green?style=for-the-badge)
![Linux](https://img.shields.io/badge/Linux-supported-orange?style=for-the-badge)
![Pthreads](https://img.shields.io/badge/POSIX-pthreads-purple?style=for-the-badge)
![Tests](https://img.shields.io/badge/tests-passing-brightgreen?style=for-the-badge)

</div>

---

## 🚀 About

**MiniDB** is a lightweight key-value database server built from scratch in **C**.

The project combines low-level networking, concurrency, synchronization, in-memory data structures, and persistent storage into a small database system that is easy to understand and experiment with.

The main goal is simple:

> **Build a database server from the ground up and understand what happens underneath.**

---

## ✨ Features

- 🌐 TCP client/server architecture
- 🧵 Multithreaded client handling
- 👷 Fixed-size thread pool
- 🗂️ Hash-table based storage
- 🔒 Per-bucket reader/writer locks
- ⏱️ Key expiration / TTL
- 📝 Write-Ahead Log (WAL)
- 📸 Atomic database snapshots
- ♻️ WAL compaction
- 💥 Crash/restart recovery
- 🛑 Graceful SIGTERM shutdown
- 🧱 Oversized-command protection
- 🧪 Concurrent stress testing
- 🛡️ AddressSanitizer + UBSan testing

---

# 🏗️ Architecture

```text
                         ┌──────────────────┐
                         │     CLIENT       │
                         │                  │
                         │   nc / TCP App   │
                         └────────┬─────────┘
                                  │
                                  │ TCP :8080
                                  ▼
                         ┌──────────────────┐
                         │    TCP SERVER    │
                         │                  │
                         │ accept() / poll  │
                         └────────┬─────────┘
                                  │
                                  ▼
                         ┌──────────────────┐
                         │   THREAD POOL    │
                         │                  │
                         │  Worker 1        │
                         │  Worker 2        │
                         │  Worker 3        │
                         │  Worker 4        │
                         └────────┬─────────┘
                                  │
                                  ▼
                         ┌──────────────────┐
                         │ COMMAND HANDLER  │
                         └────────┬─────────┘
                                  │
                                  ▼
                   ┌────────────────────────────┐
                   │         HASH TABLE         │
                   │                            │
                   │ Bucket 0  🔒 RW Lock      │
                   │ Bucket 1  🔒 RW Lock      │
                   │ Bucket 2  🔒 RW Lock      │
                   │ ...                        │
                   │ Bucket 9  🔒 RW Lock      │
                   └─────────────┬──────────────┘
                                 │
                    ┌────────────┴────────────┐
                    │                         │
                    ▼                         ▼
             ┌──────────────┐         ┌──────────────┐
             │     WAL      │         │   SNAPSHOT   │
             │ Persistence  │         │ + COMPACTION │
             └──────────────┘         └──────────────┘

```
---

# 🔄 Request Flow

```text
Client
  │
  │ TCP connection
  ▼
Server
  │
  │ accept()
  ▼
Thread Pool
  │
  ▼
Client Handler
  │
  ▼
Command Parser
  │
  ▼
Hash Table
  │
  ├──────────────► WAL
  │
  ▼
Response
  │
  ▼
Client
```

---

# 📖 Commands

| Command                    | Description                   |
| -------------------------- | ----------------------------- |
| `SET key value`            | Store a value                 |
| `SET key value EX seconds` | Store a value with expiration |
| `GET key`                  | Retrieve a value              |
| `DEL key`                  | Delete a key                  |
| `EXISTS key`               | Check whether a key exists    |
| `SAVE`                     | Create a snapshot             |
| `FLUSHDB`                  | Delete all keys               |
| `INFO`                     | Display database information  |
| `PING`                     | Test the connection           |
| `HELP`                     | Display available commands    |
| `EXIT`                     | Close the connection          |

---

# 💻 Quick Start

## 1. Clone

```bash
git clone <your-repository-url>
cd MiniDB
```

## 2. Build

```bash
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug
```

## 3. Start the server

```bash
./cmake-build-debug/MiniDB
```

MiniDB listens on:

```text
127.0.0.1:8080
```

## 4. Connect

Using `netcat`:

```bash
nc 127.0.0.1 8080
```

---

# 🎮 Example

```text
PING
PONG

SET name MiniDB
OK

GET name
MiniDB

EXISTS name
1

SET temporary hello EX 5
OK

GET temporary
hello

DEL name
1

GET name
(nil)

EXIT
BYE
```

---

# ⏱️ Expiration

Keys can be created with an expiration time:

```text
SET session abc123 EX 10
```

The key is automatically removed after the expiration period.

```text
GET session
abc123
```

After expiration:

```text
GET session
(nil)
```

MiniDB uses a background expiration thread while database operations also check for expired entries.

---

# 💾 Persistence

MiniDB uses a **Write-Ahead Log (WAL)** to persist database mutations.

A simplified write path looks like this:

```text
              SET key value
                    │
             ┌──────┴──────┐
             ▼             ▼
        Hash Table        WAL
             │             │
             │             ▼
             │        Persistent
             │          Storage
             ▼
        Current State
```

When the server starts:

```text
       ┌──────────────┐
       │   Snapshot   │
       └──────┬───────┘
              │
              ▼
        Load database
              │
              ▼
         Replay WAL
              │
              ▼
        Server Ready
```

---

# 📸 Snapshots

The `SAVE` command creates a snapshot of the current database state.

```text
Database
   │
   ▼
snapshot.tmp
   │
   │ fsync()
   ▼
atomic rename()
   │
   ▼
snapshot
```

Temporary files and atomic rename are used to avoid replacing the existing snapshot with an incomplete file.

---

# ♻️ Compaction

As the WAL grows, MiniDB can compact its persistent state.

```text
        Large WAL
            │
            ▼
      Create Snapshot
            │
            ▼
       Reset WAL
            │
            ▼
   ┌───────────────────┐
   │ Snapshot + Small  │
   │       WAL         │
   └───────────────────┘
```

This keeps recovery time and WAL size under control.

---

# 💥 Crash Recovery

MiniDB has been tested against abrupt server termination and restart.

The recovery process is:

```text
Write Data
    │
    ▼
Save Snapshot
    │
    ▼
Write WAL Entries
    │
    ▼
   💥 CRASH
    │
    ▼
Restart Server
    │
    ▼
Load Snapshot
    │
    ▼
Replay WAL
    │
    ▼
Data Recovered ✓
```

The crash/recovery test verifies that previously written data is restored correctly after an unexpected termination.

---

# 🧵 Concurrency

MiniDB uses a fixed-size thread pool for handling clients.

```text
                  ┌─────────────┐
                  │ Thread Pool │
                  └──────┬──────┘
                         │
          ┌──────────────┼──────────────┐
          ▼              ▼              ▼
       Worker 1       Worker 2       Worker 3
          │              │              │
          ▼              ▼              ▼
       Client A       Client B       Client C
```

The database uses a separate reader/writer lock for each hash-table bucket.

Instead of one global lock:

```text
Database
    │
    🔒
    │
Everything blocked
```

MiniDB uses:

```text
Bucket 0 ── 🔒
Bucket 1 ── 🔒
Bucket 2 ── 🔒
Bucket 3 ── 🔒
...
Bucket 9 ── 🔒
```

This allows operations affecting different buckets to execute concurrently.

---

# 🛑 Graceful Shutdown

MiniDB handles `SIGTERM` and shuts down active connections safely.

```text
SIGTERM
   │
   ▼
Stop accepting clients
   │
   ▼
Shutdown active sockets
   │
   ▼
Workers exit
   │
   ▼
Destroy thread pool
   │
   ▼
Close WAL
   │
   ▼
Destroy database
   │
   ▼
Shutdown complete ✓
```

The worker threads retain ownership of closing their client sockets, which avoids file-descriptor reuse problems during shutdown.

---

# 🧱 Command Safety

MiniDB protects the server from oversized commands.

If a command exceeds the configured client buffer:

```text
Huge Command
     │
     ▼
ERR command too long
     │
     ▼
Discard remaining command
     │
     ▼
Continue reading
     │
     ▼
Next command processed normally
```

This prevents malformed or oversized input from corrupting command framing.

---

# 🧪 Testing

MiniDB includes automated tests for its core components.

Run:

```bash
ctest --test-dir cmake-build-debug --output-on-failure
```

Current test coverage includes:

```text
✓ WAL
✓ Snapshot persistence
✓ Key expiration
✓ Expiration stress
✓ Database compaction
```

---

# 🔥 Stress Testing

The project includes a concurrent stress test.

Start MiniDB:

```bash
./cmake-build-debug/MiniDB
```

Then run:

```bash
./stress.sh
```

Example:

```text
Clients: 20
Operations: 100 per client
Total: 2000

Stress test finished.
Errors: 0
Successful SETs: 2000

Stress test PASSED.
```

---

# 🛡️ Sanitizer Testing

MiniDB has been tested with:

* AddressSanitizer
* UndefinedBehaviorSanitizer

Create a sanitizer build:

```bash
cmake -S . -B cmake-build-sanitize \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
```

Build:

```bash
cmake --build cmake-build-sanitize
```

Run the tests:

```bash
ctest --test-dir cmake-build-sanitize --output-on-failure
```

---

# 📁 Project Structure

```text
MiniDB/
│
├── CMakeLists.txt
├── README.md
│
├── stress.sh
├── compaction_crash_recovery_test.sh
│
├── src/
│   ├── main.c
│   ├── server.c
│   ├── database.c
│   ├── thread_pool.c
│   ├── wal.c
│   └── persistence.c
│
└── tests/
    ├── wal_test.c
    ├── persistence_test.c
    ├── expiration_test.c
    ├── expiration_stress_test.c
    └── compaction_test.c
```

---

# 🧩 Components

### `main.c`

Application entry point.

Responsible for:

* Database initialization
* WAL initialization
* Snapshot loading
* WAL replay
* Server startup
* Graceful shutdown

### `server.c`

Responsible for:

* TCP socket creation
* Listening
* Accepting clients
* Server lifecycle
* Signal-aware shutdown

### `thread_pool.c`

Responsible for:

* Worker threads
* Task queue
* Client handling
* Active client tracking
* Worker shutdown

### `database.c`

Responsible for:

* Hash table
* GET / SET / DEL / EXISTS
* Expiration
* Bucket locks
* Database lifecycle

### `wal.c`

Responsible for:

* WAL creation
* WAL logging
* WAL flushing
* WAL reset
* Durable file replacement

### `persistence.c`

Responsible for:

* Snapshot creation
* Snapshot loading
* Database compaction
* Atomic snapshot replacement

---

# 🎯 Design Philosophy

MiniDB is intentionally small.

The project prioritizes:

### Understandability

The complete system can be followed from:

```text
TCP connection
      ↓
Command
      ↓
Thread
      ↓
Hash table
      ↓
WAL
      ↓
Disk
```

### Correctness

Important failure cases are explicitly considered:

```text
Client disconnect
Oversized command
Concurrent access
Key expiration
Server shutdown
WAL reset
Snapshot replacement
Process crash
Server restart
```

### Learning

MiniDB provides practical experience with:

```text
C11
POSIX sockets
pthread
Mutexes
Condition variables
Reader/writer locks
Hash tables
File I/O
fsync()
rename()
Signals
Concurrency
Persistence
Crash recovery
```

---

# 📊 Status

## MiniDB v1.0

| Component           | Status |
| ------------------- | :----: |
| TCP server          |    ✅   |
| Client handling     |    ✅   |
| Thread pool         |    ✅   |
| Hash table          |    ✅   |
| Per-bucket RW locks |    ✅   |
| Key expiration      |    ✅   |
| WAL persistence     |    ✅   |
| Snapshots           |    ✅   |
| Compaction          |    ✅   |
| Crash recovery      |    ✅   |
| Graceful shutdown   |    ✅   |
| Oversized commands  |    ✅   |
| Stress testing      |    ✅   |
| ASan / UBSan        |    ✅   |

---

# 🔮 Future Ideas

MiniDB is currently considered a stable **v1.0 checkpoint**.

Possible future improvements:

* Dynamic hash-table resizing
* `TTL`
* `PERSIST`
* `INCR`
* `DECR`
* More database commands
* Benchmarking
* Latency measurements
* Configurable server settings
* More failure-injection tests
* Richer client protocol
* Client libraries

These are intentionally left for future development.

---

# 🛠️ Tech Stack

```text
Language          C11
Build System      CMake
Networking        POSIX TCP sockets
Concurrency       POSIX pthreads
Synchronization   pthread mutex / cond / RW locks
Storage           Hash table
Persistence       WAL + snapshots
Testing           CTest
Debugging         AddressSanitizer + UBSan
Platform          Linux
```

---

# ❤️ Why MiniDB?

MiniDB is a small project with a big purpose:

> **Understand how a database server actually works.**

Rather than hiding everything behind frameworks, MiniDB exposes the important systems underneath:

```text
             ┌────────────────┐
             │   Networking   │
             └───────┬────────┘
                     │
             ┌───────▼────────┐
             │   Concurrency  │
             └───────┬────────┘
                     │
             ┌───────▼────────┐
             │ Data Structures │
             └───────┬────────┘
                     │
             ┌───────▼────────┐
             │   Persistence  │
             └───────┬────────┘
                     │
             ┌───────▼────────┐
             │ Crash Recovery │
             └───────┬────────┘
                     │
                     ▼
                  MiniDB
```

---

<div align="center">

### 🗄️ MiniDB

**Small enough to understand.
Complex enough to teach real systems programming.**

<br>

Made with ❤️ in C

</div>
