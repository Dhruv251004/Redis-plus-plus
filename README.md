# redis++

A from-scratch C++17 reimplementation of a Redis-like in-memory key-value server. It speaks the RESP (REdis Serialization Protocol), so it can be talked to with `redis-cli` or any standard Redis client. Built as a learning project to understand how Redis works under the hood: raw sockets, an epoll event loop, protocol parsing, and command dispatch.

## Features

- TCP server built on raw POSIX sockets (`socket`/`bind`/`listen`/`accept`)
- Single-threaded, non-blocking `epoll` event loop handling many concurrent client connections
- Per-connection buffering to correctly reassemble RESP messages split across multiple reads
- Incremental RESP parser/serializer (simple strings, bulk strings, integers, arrays, errors, null bulk strings)
- In-memory key-value store (`unordered_map<string, string>`) with per-key expiry
- Commands: `PING`, `ECHO`, `SET` (with `EX`), `GET`, `DEL`, `EXISTS`, `EXPIRE`, `TTL`, `PERSIST`, `EXPIREAT`, `PEXPIREAT`
- Persistence:
  - **AOF** (`data/appendonly.aof`): every successful write command is appended and replayed on startup
  - **RDB snapshot** (`data/dump.rdb`): the whole dataset is saved every 300 seconds, in a custom binary format with a CRC32 checksum. The snapshot is written to a temp file, fsynced, then atomically renamed over the old one. After a successful snapshot the AOF is truncated
  - On startup the snapshot is validated and loaded first, then the AOF is replayed on top of it
- Leveled logging with [spdlog](https://github.com/gabime/spdlog) (`error` for failures, `warn` for bad input and corrupted data, `info` for lifecycle events). `warn` and `info` messages are tagged `[file::Class::function]`
- Graceful-ish handling of client disconnects, partial reads, and protocol errors

Listens on the standard Redis port **6379**.

## Project layout

```
src/
  main.cpp                Entry point — starts the server
  network/                Raw TCP socket handling and per-client connection buffers
  protocol/                RESP value model, streaming parser, and serializer
  utility/                 Small parsing/string helper functions
  command/                 Command dispatch (CommandExecutor)
  database/                In-memory key-value store
  persistence/             AOF, RDB snapshots, and the ByteBuffer binary serializer
  constants/               Shared constants (data directory and file names)
  server/                  RedisServer (epoll event loop, snapshot timer) and ConnectionManager
data/                      AOF and RDB files are written here (git-ignored)
```

## Building

Requires CMake 3.16+, a C++17 compiler, and zlib (for CRC32; `sudo apt install zlib1g-dev` on Debian/Ubuntu). Linux/POSIX only (uses `epoll`, so it won't build natively on Windows — use WSL or a Linux container).

spdlog is fetched automatically by CMake on the first configure (needs network), or a system install (`libspdlog-dev`) is used if present.

```bash
mkdir -p build data
cd build
cmake ..
cmake --build .
cd ..
./build/redispp
```

Run the server from the project root: it reads and writes its files in a `data/` directory relative to the working directory, and that directory must exist.

## Trying it out

With the server running, connect using `redis-cli` (or `nc`/`telnet` with raw RESP):

```bash
redis-cli -p 6379 PING
redis-cli -p 6379 SET foo bar
redis-cli -p 6379 GET foo
redis-cli -p 6379 ECHO hello
redis-cli -p 6379 SET session abc EX 60
redis-cli -p 6379 TTL session
```

Stop the server, start it again, and your keys are still there. You can also benchmark it, e.g. `redis-benchmark -p 6379 -n 10000 -c 50 -r 100000 -t set,get`. Use `-r` so keys are randomized.

Known limitation: only RESP array requests are accepted, so inline commands (plain text such as `PING\r\n`, which `redis-benchmark -t ping` also sends) are rejected with `ERR expected array`.

## Roadmap

This is a work in progress, built up incrementally. Planned next:

- Inline command support
- Configurable snapshot interval, log level, and data directory
- Additional commands and data types (lists, hashes, sets, etc.)
- Configuration options (port, config file)
- Tests

## Non-goals (for now)

- Clustering / replication
- Authentication / ACLs
- Multi-threading
