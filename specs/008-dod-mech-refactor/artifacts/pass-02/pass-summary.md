# pass-02 summary: subscription broker locality + index migration

## Stage 1: Analysis findings

- Target subsystem: `SubscriptionBroker::Impl` hot-path operations in `subscribe()`, `unsubscribe()`, `disconnect_session()`, and `publish()`.
- Current traversal and lookup profile:
  - `publish()` iterates the full `subscriptions` hash map and branches on `record.topic == topic` for every entry.
  - `unsubscribe()` and `disconnect_session()` perform repeated string-key hash lookups and vector erase scans by subscription ID.
- Migration constraints:
  - Preserve external behavior for topic matching, delivery fan-out, and auth-session revocation semantics.
  - Keep lock boundaries unchanged (collect under lock, invoke handlers outside lock).
  - Preserve no-op behavior for unknown subscription IDs/sessions.

## Stage 2: Data layout plan (SoA/index-oriented)

### Before

- Primary state is pointer/string-key oriented:
  - `subscriptions`: `unordered_map<subscription_id, SubscriptionRecord>`.
  - `session_index`: `unordered_map<session_id, vector<subscription_id>>`.
- Publish path requires full map walk and per-record topic branch checks.

### After (planned)

- Introduce contiguous, index-addressable storage for subscription fields:
  - Parallel arrays for `subscription_id`, `session_id`, `topic`, and `handler`.
  - `subscription_id -> index` and `session_id -> vector<index>` lookup maps for stable external APIs.
- Add a topic index (`topic -> vector<index>`) to avoid full-scan publish traversal.
- Use an explicit invalid-index sentinel for stale entries and guard clauses before array access.

### Index mapping rules

- `index` is the authoritative logical record identity within contiguous arrays.
- All secondary indexes (`subscription_id`, `session_id`, `topic`) store record indexes, never direct pointers.
- Removal path uses tombstones or swap-with-last semantics with index-fixup to keep contiguous iteration efficient.


