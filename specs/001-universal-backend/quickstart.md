# Quickstart Guide: Universal Application Server Backend

**Purpose**: Step-by-step guide for developers to build and use the Isched Universal Backend.  
**Target Audience**: Frontend and backend developers familiar with GraphQL clients and C++ builds.  
**Prerequisites**: Conan 2.x, CMake, and a C++23-capable compiler installed.

## Building from Source

### First-Time Setup (run once per machine)

```bash
conan profile detect
```

### Configure and Build

Use the provided `configure.py` script — it automates all Conan and CMake steps:

```bash
python3 configure.py
```

This script runs the following steps internally:

```bash
# 1. Install Conan-managed dependencies and generate CMake integration files
conan install . -of cmake-build-debug -s build_type=Debug --build=missing

# 2. Configure CMake using the Conan-generated toolchain (Linux, Ninja)
cmake . -B ./cmake-build-debug \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake-build-debug/conan_toolchain.cmake \
  -DCMAKE_POLICY_DEFAULT_CMP0091=NEW \
  -DCMAKE_BUILD_TYPE=Debug

# 3. Build
cmake --build ./cmake-build-debug/
```

### Run Tests

```bash
cd cmake-build-debug && ctest --output-on-failure
```

### Build a Specific Target

```bash
cmake --build ./cmake-build-debug/ --target isched_graphql_tests
```

### Regenerate API Documentation

```bash
cmake --build ./cmake-build-debug/ --target docs
```

The built server binary is `cmake-build-debug/isched_srv`.

---

## Overview

Isched provides a GraphQL-native backend. You start one server, talk to one endpoint, and configure behavior through GraphQL mutations. No scripting runtimes, CLI coordinators, or external management APIs are required.

## Quick Start (5 Minutes)

### Step 1: Start the Server

```bash
isched-server --port 8080 --data-dir ./isched-data
```

The GraphQL HTTP endpoint is now available at `http://localhost:8080/graphql`.

The GraphQL WebSocket endpoint is available at `ws://localhost:8080/graphql` using the `graphql-transport-ws` subprotocol.

### Step 2: Verify the Built-In Schema

Send a GraphQL query over HTTP:

```bash
curl -X POST http://localhost:8080/graphql \
  -H "Content-Type: application/json" \
  -d '{
    "query": "query { hello version uptime health { status timestamp } }"
  }'
```

### Step 3: Apply Tenant Configuration

Use a GraphQL mutation to create or update a tenant configuration snapshot.

```bash
curl -X POST http://localhost:8080/graphql \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer ADMIN_TOKEN" \
  -d '{
    "query": "mutation ApplyConfiguration($input: ApplyConfigurationInput!) { applyConfiguration(input: $input) { configuration { id version isActive } warnings } }",
    "variables": {
      "input": {
        "tenantId": "tenant-demo",
        "displayName": "Demo configuration",
        "models": [
          {
            "name": "User",
            "fields": [
              {"name": "email", "type": "String!", "unique": true},
              {"name": "displayName", "type": "String!"},
              {"name": "createdAt", "type": "DateTime!"}
            ]
          },
          {
            "name": "Product",
            "fields": [
              {"name": "name", "type": "String!"},
              {"name": "price", "type": "Float!"},
              {"name": "inStock", "type": "Boolean!"}
            ]
          }
        ],
        "auth": {
          "allowRegistration": true,
          "jwtIssuer": "isched-local"
        }
      }
    }
  }'
```

### Step 4: Query the Configured Schema

```bash
curl -X POST http://localhost:8080/graphql \
  -H "Content-Type: application/json" \
  -d '{
    "query": "query { users { id email displayName } products { id name price inStock } }"
  }'
```

## Authentication Flow

### Register a User

```graphql
mutation {
  register(input: {
    tenantId: "tenant-demo"
    email: "user@example.com"
    password: "securepassword"
    displayName: "Demo User"
  }) {
    accessToken
    refreshToken
    user {
      id
      email
      displayName
    }
  }
}
```

### Log In

```graphql
mutation {
  login(input: {
    tenantId: "tenant-demo"
    email: "user@example.com"
    password: "securepassword"
  }) {
    accessToken
    refreshToken
    expiresAt
  }
}
```

### Use the Token on HTTP Requests

```bash
curl -X POST http://localhost:8080/graphql \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_JWT_TOKEN" \
  -d '{
    "query": "query { currentUser { id email displayName } }"
  }'
```

## Real-Time Updates Over WebSocket

### Example Client

```javascript
import { createClient } from 'graphql-ws';

const client = createClient({
  url: 'ws://localhost:8080/graphql',
  connectionParams: {
    authorization: 'Bearer YOUR_JWT_TOKEN'
  }
});

const dispose = client.subscribe(
  {
    query: `subscription {
      configurationApplied(tenantId: "tenant-demo") {
        tenantId
        version
        activatedAt
      }
    }`
  },
  {
    next: (value) => console.log('event', value),
    error: (error) => console.error(error),
    complete: () => console.log('done')
  }
);
```

## Built-In Operations

### Health and Server Info

```graphql
query {
  serverInfo {
    version
    startedAt
    activeTenants
    transportModes
  }
  health {
    status
    timestamp
    components {
      name
      status
      details
    }
  }
}
```

### Inspect Active Configuration

```graphql
query {
  activeConfiguration(tenantId: "tenant-demo") {
    id
    version
    displayName
    isActive
    schemaSdl
  }
}
```

## Failure Handling

- Invalid configuration mutations fail without replacing the active snapshot.
- Destructive schema changes are rejected until an explicit migration path is supplied.
- WebSocket clients should reconnect and resubscribe after disconnect.
