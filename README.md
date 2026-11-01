# redis++

A from-scratch C++17 reimplementation of a Redis-like in-memory key-value server. It speaks the RESP (REdis Serialization Protocol), so it can be talked to with `redis-cli` or any standard Redis client. Built as a learning project to understand how Redis works under the hood: raw sockets, an epoll event loop, protocol parsing, and command dispatch.

## Features

- TCP server built on raw POSIX sockets (`socket`/`bind`/`listen`/`accept`)
- Single-threaded, non-blocking `epoll` event loop handling many concurrent client connections
- Per-connection buffering to correctly reassemble RESP messages split across multiple reads
- Incremental RESP parser/serializer (simple strings, bulk strings, integers, arrays, errors, null bulk strings)
- In-memory key-value store (`unordered_map<string, string>`) with per-key expiry. Expired keys are removed lazily when accessed and by an active sweep that runs every 100 ms in small bounded batches, so keys nobody reads don't pile up
- Commands: `PING`, `ECHO`, `SET` (with `EX`), `GET`, `DEL`, `EXISTS`, `EXPIRE`, `TTL`, `PERSIST`, `EXPIREAT`, `PEXPIREAT`
- Transactions: `MULTI`, `EXEC`, `DISCARD`. Commands are queued per connection and run together on `EXEC`; an unknown command inside `MULTI` aborts the transaction (`EXECABORT`)
- Pub/sub: `PUBLISH`, `SUBSCRIBE`, `UNSUBSCRIBE`. Subscriptions are cleaned up when a client disconnects
- Persistence:
  - **AOF** (`data/appendonly.aof`): every write that actually changed data is appended and replayed on startup. Queued, failed, and no-op writes are not logged. Relative times (`SET ... EX`, `EXPIRE`) are stored as an absolute `PEXPIREAT`, and a transaction is stored as `MULTI ... EXEC`, so a crash halfway through never applies it partially
  - **RDB snapshot** (`data/dump.rdb`): the whole dataset is saved every 300 seconds, in a custom binary format with a CRC32 checksum. The snapshot is written to a temp file, fsynced, then atomically renamed over the old one. After a successful snapshot the AOF is truncated
  - On startup the snapshot is validated and loaded first, then the AOF is replayed on top of it
- Leveled logging with [spdlog](https://github.com/gabime/spdlog) (`error` for failures, `warn` for bad input and corrupted data, `info` for lifecycle events). Every message is tagged with where it came from as `[file::Class::function]`, e.g. `[RDB.cpp::RDB::write] failed to rename ...`
- Graceful-ish handling of client disconnects, partial reads, and protocol errors
- Unit tests with GoogleTest for the database, RESP parser and serializer, command executor, AOF, RDB, and byte buffer
- Dockerfile for a small multi-stage image

Listens on the standard Redis port **6379**.

## Project layout

```
src/
  main.cpp                Entry point — starts the server
  network/                Raw TCP socket handling and per-client connection buffers
  protocol/                RESP value model, streaming parser, and serializer
  utility/                 Small parsing/string helper functions
  command/                 Command dispatch and transactions (CommandExecutor)
  database/                In-memory key-value store with the expiry index
  persistence/             AOF, AOF entry encoding, RDB snapshots, and the ByteBuffer binary serializer
  pubsub/                  PubSubManager (channel subscriptions and message delivery)
  constants/               Shared constants (data directory and file names)
  server/                  RedisServer (epoll event loop, snapshot and expiry timers) and ConnectionManager
tests/                     GoogleTest unit tests, one executable per area
data/                      AOF and RDB files are written here (git-ignored)
Dockerfile                 Multi-stage build of the server image
```

## Building

Requires CMake 3.16+, a C++17 compiler, zlib (for CRC32), and GoogleTest (the test targets are always configured): `sudo apt install zlib1g-dev libgtest-dev` on Debian/Ubuntu. Linux/POSIX only (uses `epoll`, so it won't build natively on Windows — use WSL or a Linux container).

spdlog is fetched automatically by CMake on the first configure (needs network), or a system install (`libspdlog-dev`) is used if present.

```bash
mkdir -p build data
cd build
cmake ..
cmake --build . --target redispp   # just the server; omit --target to build the tests too
cd ..
./build/redispp
```

Run the server from the project root: it reads and writes its files in a `data/` directory relative to the working directory, and that directory must exist.

## Running with Docker

```bash
docker build -t redispp .
docker run --rm --init -p 6379:6379 -v redispp-data:/app/data redispp
```

The image is built in two stages, so the final image contains only the server binary and zlib. The named volume keeps the AOF and RDB files when the container is removed. `--init` is recommended because the server doesn't handle `SIGTERM` yet (see the roadmap), so `docker stop` would otherwise wait for the timeout before killing it.

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

Transactions need a single connection, so use an interactive `redis-cli` session (each `redis-cli -p 6379 ...` call above is a separate connection):

```
$ redis-cli -p 6379
127.0.0.1:6379> MULTI
OK
127.0.0.1:6379> SET a 1
QUEUED
127.0.0.1:6379> SET b 2 EX 100
QUEUED
127.0.0.1:6379> EXEC
1) OK
2) OK
```

Pub/sub works across two terminals:

```bash
redis-cli -p 6379 SUBSCRIBE news      # terminal 1
redis-cli -p 6379 PUBLISH news hello  # terminal 2
```

Stop the server, start it again, and your keys are still there. You can also benchmark it, e.g. `redis-benchmark -p 6379 -n 10000 -c 50 -r 100000 -t set,get`. Use `-r` so keys are randomized.

## Tests

```bash
cmake --build build
cd build && ctest --output-on-failure
```

Each area has its own test executable (`db_tests`, `resp_parser_tests`, `resp_serializer_tests`, `command_executor_tests`, `aof_tests`, `aof_entry_tests`, `rdb_tests`, `byte_buffer_tests`), so you can also run one directly, e.g. `./build/db_tests --gtest_filter='DatabasePurgeTest.*'`. The AOF and RDB tests work in a temporary directory and never touch your real `data/` files.

## Known limitations

- Only RESP array requests are accepted, so inline commands (plain text such as `PING\r\n`, which `redis-benchmark -t ping` also sends) are rejected with `ERR expected array`.
- `SUBSCRIBE` and `UNSUBSCRIBE` take a single channel per call, and a subscribed client can still run normal commands.
- Snapshots (RDB) store string values only.
- The server doesn't handle `SIGINT`/`SIGTERM`, so stopping it doesn't take a final snapshot.

## Roadmap

This is a work in progress, built up incrementally. Planned next:

- Graceful shutdown (final snapshot and AOF fsync on `SIGINT`/`SIGTERM`)
- Inline command support
- Configurable snapshot interval, log level, and data directory
- Additional commands and data types (lists, hashes, sets, etc.)
- Configuration options (port, config file)
- Multi-channel `SUBSCRIBE`/`UNSUBSCRIBE`

## Non-goals (for now)

- Clustering / replication
- Authentication / ACLs
- Multi-threading
