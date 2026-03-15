# Isched — Deployment Guide

This guide covers production and embedded deployment of `isched`, a single-process, multi-tenant GraphQL server built on [cpp-httplib](https://github.com/yhirose/cpp-httplib) and Boost.Beast.

---

## Table of Contents

1. [Overview](#overview)
2. [HTTP and WebSocket Configuration](#http-and-websocket-configuration)
3. [TLS / HTTPS](#tls--https)
4. [Multi-Tenant Bootstrap](#multi-tenant-bootstrap)
5. [Graceful Shutdown](#graceful-shutdown)
6. [Embedded / Raspberry Pi Deployment](#embedded--raspberry-pi-deployment)

---

## Overview

`isched` exposes a single endpoint at `/graphql` that serves:

- **HTTP POST** — standard GraphQL query / mutation transport  
- **WebSocket** — `graphql-transport-ws` protocol for subscriptions (default port: `http_port + 1`)

There is no REST API, IPC interface, or scripting surface.  All management operations — including tenant provisioning, user management, and configuration — are performed exclusively through GraphQL mutations.

---

## HTTP and WebSocket Configuration

The server is configured via `Server::Configuration`:

```cpp
#include "isched_Server.hpp"

using isched::v0_0_1::backend::Server;

Server::Configuration cfg;
cfg.host        = "0.0.0.0";   // bind address (default: "localhost")
cfg.port        = 8080;        // HTTP port (default: 8080)
cfg.ws_port     = 8081;        // WebSocket port (0 = port + 1)
cfg.min_threads = 4;           // minimum thread-pool workers (default: 4)
cfg.max_threads = 100;         // maximum thread-pool workers (default: 100)

// JWT secret — MUST be overridden in production (≥ 32 bytes)
cfg.jwt_secret_key = "change-me-in-production-at-least-32-bytes";

auto server = Server::create(cfg);
server->start();
```

| Field | Default | Notes |
|-------|---------|-------|
| `host` | `"localhost"` | Loopback only by default. Use `"0.0.0.0"` for all interfaces. |
| `port` | `8080` | HTTP/GraphQL port. |
| `ws_port` | `0` | `0` means `port + 1`. Set explicitly to separate HTTP and WS ports. |
| `min_threads` | `4` | Thread pool lower bound. Reduce to `2` on low-memory hardware. |
| `max_threads` | `100` | Thread pool upper bound. Reduce to `8–16` on Raspberry Pi. |
| `jwt_secret_key` | (auto-generated) | Random secret generated at startup if empty. Production deployments **must** set a stable value so tokens survive restarts. |
| `work_directory` | `"./data"` | Root for per-tenant SQLite files. Must be writable at runtime. |
| `enable_introspection` | `true` | Disable in production if the GraphQL schema should remain private. |

The log level is controlled by the `SPDLOG_LEVEL` environment variable (e.g. `SPDLOG_LEVEL=info`).

---

## TLS / HTTPS

`isched` is compiled with `CPPHTTPLIB_OPENSSL_SUPPORT` and ships the necessary OpenSSL linkage.  Two deployment patterns are supported:

### Option A — Reverse proxy (recommended for production)

Place `isched` behind nginx, Caddy, or HAProxy and let the proxy handle TLS termination.  `isched` listens on `localhost` only:

```
cfg.host = "127.0.0.1";
cfg.port = 8080;
```

Example minimal **nginx** snippet:

```nginx
server {
    listen 443 ssl;
    ssl_certificate     /etc/letsencrypt/live/example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/example.com/privkey.pem;

    location /graphql {
        proxy_pass          http://127.0.0.1:8080/graphql;
        proxy_http_version  1.1;
        proxy_set_header    Upgrade    $http_upgrade;
        proxy_set_header    Connection "upgrade";
    }
}
```

### Option B — Direct TLS with `httplib::SSLServer`

For embedded deployments where a reverse proxy is impractical, swap the transport in `isched_Server.cpp`:

```cpp
// Replace:
http_server = std::make_unique<httplib::Server>();

// With (requires certificate and private-key PEM files):
http_server = std::make_unique<httplib::SSLServer>(
    "/path/to/cert.pem",
    "/path/to/key.pem");
```

Generate a self-signed certificate for development:

```bash
openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem \
    -days 365 -nodes -subj "/CN=isched"
```

The WebSocket listener (`isched_Server.cpp`, `WsListener`) uses raw Boost.Beast sockets.  Enable Boost.Beast SSL by adding `boost::asio::ssl::context` to the acceptor — consult the [Boost.Beast SSL examples](https://www.boost.org/doc/libs/1_84_0/libs/beast/example/websocket/server/async-ssl/) for the pattern.

---

## Multi-Tenant Bootstrap

Isched is multi-tenant out of the box.  Each organisation (tenant) owns an independent SQLite file under `work_directory/tenants/<org-id>/`.

### Step 1 — Create the initial platform admin

The platform-admin account gates all tenant-management mutations.  On first deployment, create it directly through the C++ API (e.g. from a startup helper binary or integration test fixture):

```cpp
#include "isched_DatabaseManager.hpp"

using isched::v0_0_1::backend::DatabaseManager;

auto db = std::make_shared<DatabaseManager>();
db->open("./data");

// Hash the password with Argon2id before storing:
auto hash = isched::v0_0_1::backend::hash_password("strong-password-here");
db->create_platform_admin("admin-001", "admin@example.com", hash, "Isched Admin");
```

> **Note:** A future `bootstrapAdmin` GraphQL mutation will allow first-run setup without a helper binary.

### Step 2 — Obtain a platform-admin token

```graphql
mutation {
  login(email: "admin@example.com", password: "strong-password-here") {
    token
    expiresAt
  }
}
```

Pass the returned `token` as a Bearer header for all subsequent admin mutations:

```
Authorization: Bearer <token>
```

### Step 3 — Create an organisation (tenant)

```graphql
mutation {
  createOrganization(input: {
    id:   "acme"
    name: "Acme Corp"
    domain: "acme.example.com"
    subscriptionTier: "standard"
    userLimit:  200
    storageLimit: 2147483648
  }) {
    id
    name
    createdAt
  }
}
```

This call provisions the per-tenant SQLite file (`data/tenants/acme/`) and registers the organisation in the system database.

### Step 4 — Create an organisation admin

```graphql
mutation {
  createUser(
    organizationId: "acme"
    input: {
      email:       "alice@acme.example.com"
      password:    "secure-pass"
      displayName: "Alice"
      roles:       ["role_tenant_admin"]
    }
  ) {
    id
    email
  }
}
```

### Step 5 — (Optional) tune per-tenant thread pool

```graphql
mutation {
  updateTenantConfig(
    organizationId: "acme"
    input: {
      minThreads: 2
      maxThreads: 8
    }
  ) {
    organizationId
  }
}
```

---

## Graceful Shutdown

The server process handles `SIGINT` and `SIGTERM` and performs an orderly shutdown via `Server::stop()`:

1. The HTTP server stops accepting new connections (`httplib::Server::stop()`).
2. The WebSocket acceptor closes (`WsListener::stop()`).
3. The Boost.Asio I/O context drains pending handlers (`io_context::stop()`).
4. The adaptive thread pool joins all workers.
5. Per-tenant SQLite WAL files are checkpointed by the RAII sqlite3 destructors.

The process exits with code `0` on clean shutdown.

To stop isched running under a process supervisor:

```bash
# systemd
systemctl stop isched

# explicit signal
kill -SIGTERM <pid>

# Docker
docker stop <container>   # sends SIGTERM with a 10-second grace period by default
```

Shutdown typically completes within the `response_timeout` window (default 20 ms) for in-flight requests.

---

## Embedded / Raspberry Pi Deployment

Isched targets embedded Linux hardware as first-class deployment environment (FR-PERF-002).  The same Conan + CMake toolchain used for desktop builds is used for cross-compilation — **no special compiler flags** are required.

### Minimum Hardware

| Resource | Minimum | Notes |
|----------|---------|-------|
| RAM | 64 MB | Sufficient for a single tenant with the default configuration. |
| Storage | 32 MB (binary) + tenant data | SQLite files grow with usage; keep 256 MB+ free for production. |
| OS | Linux (any ABI) | Tested on Raspberry Pi OS (arm64, armv7l) and Yocto. |
| CPU | ARMv7 or aarch64 | SSE/AVX not required; the code is portable C++23. |

### Cross-Compilation (aarch64 target from x86-64 host)

**1. Create a Conan cross profile** (`~/.conan2/profiles/rpi-arm64`):

```ini
[settings]
os=Linux
arch=armv8
compiler=gcc
compiler.version=12
compiler.libcxx=libstdc++11
build_type=Release

[buildenv]
CC=aarch64-linux-gnu-gcc
CXX=aarch64-linux-gnu-g++
```

**2. Install a cross toolchain**:

```bash
# Ubuntu / Debian
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

**3. Install dependencies and configure**:

```bash
conan install . \
    -of cmake-build-rpi \
    -pr:b default \
    -pr:h rpi-arm64 \
    --build=missing

cmake . -B cmake-build-rpi \
    -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=cmake-build-rpi/conan_toolchain.cmake \
    -DCMAKE_POLICY_DEFAULT_CMP0091=NEW \
    -DCMAKE_BUILD_TYPE=Release

cmake --build cmake-build-rpi/
```

The `-pr:b default` flag tells Conan to build host tools (CMake, Ninja) for the build machine while cross-compiling project sources for the target.

**4. Transfer and run** the resulting `isched` binary to the Pi over SSH.

### Tuning for Low-Memory Environments

**Reduce thread pool size** via `Server::Configuration`:

```cpp
cfg.min_threads = 2;   // minimum workers
cfg.max_threads = 8;   // cap concurrency
```

Or per-tenant via the `updateTenantConfig` mutation (`minThreads`, `maxThreads`).

**SQLite page-cache tuning** — override the default 8 MB cache by issuing `PRAGMA cache_size` after opening the database.  Recommended values for constrained devices:

| Available RAM | `cache_size` (pages) | Approximate cache size |
|---|---|---|
| 64 MB | 500 | ~2 MB |
| 128 MB | 2000 | ~8 MB (default) |
| 256 MB+ | 4000–8000 | ~16–32 MB |

The code already sets `journal_mode=WAL` and `synchronous=NORMAL`, which are the recommended settings for flash storage (wear-levelling friendly, no full-sync on every write).

**Limit tenant count** via `TenantManager::Configuration::max_tenants`.  A Raspberry Pi 4 (4 GB) can comfortably handle tens of concurrent tenants; a Pi Zero (512 MB) works best with a single tenant.
