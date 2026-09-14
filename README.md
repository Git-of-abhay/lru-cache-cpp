# LRU Cache · Modern C++

[![C++ CI](https://github.com/Git-of-abhay/lru-cache-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/Git-of-abhay/lru-cache-cpp/actions/workflows/ci.yml)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)
![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)

A bounded **least recently used cache** with average O(1) lookup, insertion, update, and eviction. Built as a complete, dependency-free C++ project: reusable headers, an interactive terminal demo, reference-model tests, concurrency support, reproducible benchmarks, and GitHub Actions.

When the cache fills up, discard the entry that has gone longest without being read or updated. A hash table finds entries; a doubly linked list remembers their recency.

```text
capacity = 3                      MRU → LRU
put(A), put(B), put(C)             C → B → A
get(A)                            A → C → B
put(D)                            D → A → C     B is evicted
get(B)                            MISS
```

## Build and run

Requires a C++20 compiler, CMake 3.20+, and a platform with standard thread support. No third-party C++ dependencies.

```bash
git clone https://github.com/Git-of-abhay/lru-cache-cpp.git
cd lru-cache-cpp
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/lru_cli --demo
./build/lru_cli
./build/lru_benchmark
```

CLI commands: `put KEY VALUE`, `get KEY`, `peek KEY`, `erase KEY`, `resize N`, `show`, `stats`, `clear`, `quit`. Keys and values are single whitespace-delimited tokens. Invalid commands leave the cache unchanged. The interactive session starts with capacity 3.

## Use the library

Add `include/` to your include path, or link the CMake `lru` interface target when adding this project as a subdirectory.

```cpp
#include <lru/cache.hpp>
#include <iostream>
#include <string>

int main() {
    lru::Cache<int, std::string> cache(2);
    cache.put(1, "one");
    cache.put(2, "two");
    if (const auto* value = cache.get(1)) std::cout << *value << '\n';
    cache.put(3, "three"); // Evicts 2; reading 1 made it most recent.
}
```

`get()` returns a borrowed **const pointer**, or `nullptr` on a miss. Do not retain it after that key is updated, erased, evicted, cleared, or the cache is destroyed. `peek()` does not change recency or statistics. For shared access, use `lru::SynchronizedCache<Key, Value>`: its `get()` returns an `std::optional<Value>` copy while holding the mutex.

## What this project demonstrates

| Area | Implementation evidence |
|---|---|
| Data structures | Hash table indexes stable doubly linked list iterators |
| Algorithms | O(1) list splice for promotion; tail removal for LRU eviction |
| Modern C++ | Templates, RAII, const-correctness, move-only values, optional, mutex guards |
| Ownership and exceptions | No manual allocation; failed index insertion rolls back the new list node |
| Correctness | Deterministic edge cases, collision stress, 100,000 reference-model operations |
| Concurrency | Coarse mutex wrapper, copied results, eight-thread stress test |
| Performance | Same seeded workloads for hash/list and vector baselines; checksums and hit counts compared |
| Engineering | CMake, GCC/Clang CI, warnings as errors, Address/UndefinedBehavior sanitizers |

## API and complexity

Let `n` be the current number of entries. Hash/equality and key/value operations are assumed constant cost for this table. Hash collisions can degrade table operations to O(n).

| Operation | Expected time | Behavior |
|---|---|---|
| `get(key)` | O(1) | Promote hit to MRU; record hit/miss |
| `peek(key)`, `contains(key)` | O(1) | Inspect without promotion or counters |
| `put(key, value)` | O(1) | Insert/update at MRU; evict LRU if necessary |
| `erase(key)` | O(1) | Remove explicitly; does not count as eviction |
| `resize(capacity)` | O(k) expected | Evict k oldest entries when shrinking |
| `clear()` | O(n) | Remove entries; retain statistics |
| `snapshot()` | O(n) | Copy pairs in MRU-to-LRU order |
| `size()`, `capacity()`, `statistics()` | O(1) | Inspect metadata |

Space is O(capacity). An insertion temporarily stages one extra entry for exception safety. Capacity 0 disables storage. Updating a key does not increase size or count as an eviction. Shrinking counts removed entries as evictions. `reset_statistics()` resets counters without changing data.

## Validation and benchmarks

```bash
# Memory and undefined behavior checks
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DLRU_SANITIZERS=ON
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure

# Optional race detection, in a separate build on a supported host
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DLRU_TSAN=ON
cmake --build build-tsan --parallel
ctest --test-dir build-tsan --output-on-failure

# CSV output: six combinations of workload and capacity
./build/lru_benchmark 200000
```

The benchmark uses uniform accesses across 4× capacity and a workload with 90% of accesses directed at a hot set of size capacity/4. It starts empty, uses seed 42, and reports time per **request** (lookup plus insertion on a miss). Generation and console output are outside the timed region; allocation and eviction are included. The vector baseline implements the same LRU behavior.

See [measured local results](docs/benchmark-results.md) and [design and correctness notes](docs/design.md). Timing is machine-dependent; this is a single-thread microbenchmark, not an application throughput guarantee. Small caches can favor a contiguous vector despite its linear asymptotic cost.

## Scope and tradeoffs

This is an in-memory, count-bounded cache. There is no persistence, TTL, byte-based budgeting, or distributed coordination. The base cache requires external synchronization when shared. The wrapper serializes operations and is intended for correctness rather than maximum concurrent throughput. Its returned values must be copyable; the base cache supports move-only values. Copy and move of the cache object itself are explicitly disabled to prevent accidental cross-container iterator ownership.

Keys must be copyable and meet `std::unordered_map` requirements. Custom hash and equality functions must not throw. See the design notes for exception guarantees and future extensions.

## Repository map

```text
include/lru/cache.hpp       Header-only cache and synchronized wrapper
src/main.cpp               Interactive CLI and deterministic demo
tests/cache_test.cpp       Edge, model, exception, and thread tests
benchmarks/benchmark.cpp   Seeded comparison against vector-based LRU
docs/                      Design explanation and measured results
.github/workflows/ci.yml   GCC/Clang builds and sanitizer checks
```

Licensed under [MIT](LICENSE).
