# redis++

A from-scratch C++17 reimplementation of a Redis-like in-memory key-value server. It speaks the RESP (REdis Serialization Protocol), so it can be talked to with `redis-cli` or any standard Redis client. Built as a learning project to understand how Redis works under the hood: raw sockets, an epoll event loop, protocol parsing, and command dispatch.

## Features

- TCP server built on raw POSIX sockets (`socket`/`bind`/`listen`/`accept`)
- Single-threaded, non-blocking `epoll` event loop handling many concurrent client connections
- Per-connection buffering to correctly reassemble RESP messages split across multiple reads
- Incremental RESP parser/serializer (simple strings, bulk strings, integers, arrays, errors, null bulk strings)
- In-memory key-value store (`unordered_map<string, string>`)
- Commands: `PING`, `ECHO`, `SET`, `GET`
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
  server/                  RedisServer (epoll event loop) and ConnectionManager
```

## Building

Requires CMake 3.16+ and a C++17 compiler. Linux/POSIX only (uses `epoll`, so it won't build natively on Windows — use WSL or a Linux container).

```bash
mkdir build && cd build
cmake ..
cmake --build .
./redispp
```

## Trying it out

With the server running, connect using `redis-cli` (or `nc`/`telnet` with raw RESP):

```bash
redis-cli -p 6379 PING
redis-cli -p 6379 SET foo bar
redis-cli -p 6379 GET foo
redis-cli -p 6379 ECHO hello
```

## Roadmap

This is a work in progress, built up incrementally. Planned next:

- `DEL` and key-existence commands
- Key expiration / TTL support
- Persistence (RDB-style snapshotting and/or AOF)
- Additional commands and data types (lists, hashes, sets, etc.)
- Configuration options (port, config file)
- Tests

## Non-goals (for now)

- Clustering / replication
- Authentication / ACLs
- Multi-threading
