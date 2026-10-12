<p align="center">
  <img src="assets/logo.jpg" alt="Ginnunga Logo" width="280" />
</p>

<h1 align="center">Project Ginnunga</h1>

<p align="center">
  <a href="https://isocpp.org/"><img src="https://img.shields.io/badge/C%2B%2B-26%20%2F%2020-blue.svg" alt="C++26" /></a>
  <a href="https://www.gnu.org/licenses/agpl-3.0"><img src="https://img.shields.io/badge/License-AGPL_v3-blue.svg" alt="License: AGPL v3" /></a>
  <img src="https://img.shields.io/badge/build-passing-brightgreen.svg" alt="Build" />
</p>

> **"In the beginning there was Ginnungagap, the primordial void, where the frost of Niflheim and the sparks of Muspelheim collided to ignite existence."**  
> — *The Prose Edda*

**Ginnunga** is a high-performance C++ Sparse Cache Algorithm Framework engineered for low-latency, mission-critical systems. It addresses environments where the addressable keyspace is massive and unpredictable, but active data is thinly distributed. Ginnunga bridges the gap between the $O(1)$ latency of dense arrays and the memory efficiency of hash maps, eliminating pointer chasing, avoiding false sharing, and keeping CPU cache lines hot.

---

## Architecture Overview

```mermaid
graph TD
    UserApp([User Application]) --> Cache["SparseCache&lt;Key, Value, Storage, Eviction, Concurrency&gt;"]
    
    subgraph Zero-Cost C++20 Concepts
        Cache --> StorageConcept["StorageConcept"]
        Cache --> EvictionConcept["EvictionConcept"]
        Cache --> ConcurrencyConcept["ConcurrencyConcept"]
        Cache --> MemoryConcept["MemoryResourceConcept"]
    end
    
    subgraph Pluggable Storage Backends
        StorageConcept --> PagedSparseArray["PagedSparseArray (Virtual Mem / mmap)"]
        StorageConcept --> SlotArrayMap["SlotArrayMap (Dense/Sparse Set)"]
        StorageConcept --> SimdFlatMap["SimdFlatMap (NEON / AVX2 Swiss Table)"]
        StorageConcept --> JudyTreeBackend["JudyTreeBackend (Compressed Trie)"]
    end
    
    subgraph Eviction Policies
        EvictionConcept --> LRU["LRUPolicy"]
        EvictionConcept --> LFU["LFUPolicy"]
        EvictionConcept --> ARC["ARCPolicy (Adaptive Replacement)"]
        EvictionConcept --> TTL["TTLPolicy (High-Res Chrono)"]
    end
    
    subgraph Wait-Free Concurrency Models
        ConcurrencyConcept --> EBR["EpochReclamation (EBR Wait-Free Reads)"]
        ConcurrencyConcept --> HP["HazardPointers (Reader Publishing)"]
        ConcurrencyConcept --> TLC["ThreadLocalCacheFront (L1 Front Ring)"]
        ConcurrencyConcept --> ST["SingleThreaded (Zero Overhead)"]
    end
```

---

## Key Features

### 1. Static Polymorphism via C++20 Concepts
Every component is decoupled using C++20 Concepts (`StorageConcept`, `EvictionConcept`, `ConcurrencyConcept`, `MemoryResourceConcept`). Zero virtual dispatch overhead, enabling total compiler inlining and interprocedural optimization.

### 2. Pluggable Sparse Storage Backends
- **`PagedSparseArray`**: Reserves massive virtual address spaces using OS paging (`mmap`). Physical memory is committed on-demand upon access and dynamically reclaimed using `madvise(MADV_DONTNEED)`. Ideal for sparse integer keys (such as FIX Order IDs).
- **`SlotArrayMap` (Sparse Set)**: Utilizes two arrays: a sparse mapping array for $O(1)$ lookups and a tightly packed dense array for $O(1)$ cache-line-friendly linear iteration and swap-and-pop deletions.
- **`SimdFlatMap`**: Open-addressing flat hash table utilizing SIMD vector instructions (ARM NEON on Apple Silicon, AVX2 on x86_64) to inspect 16 control bytes simultaneously in a single CPU cycle.
- **`JudyTreeBackend`**: Highly compressed adaptive radix trie designed to minimize memory footprint across sparse integer keys without pointer chasing.

### 3. Advanced Replacement & Eviction Policies
- **`LRUPolicy`**: Least Recently Used recency tracking in $O(1)$ time.
- **`LFUPolicy`**: Least Frequently Used frequency bucketing in $O(1)$ time.
- **`ARCPolicy`**: Adaptive Replacement Cache dynamically self-tuning between recency and frequency using ghost caches ($B_1$, $B_2$).
- **`TTLPolicy`**: Time-to-Live policy evaluating monotonic clocks to expire stale keys lazily and proactively.

### 4. Wait-Free Concurrency & Memory Reclamation
- **`EpochReclamation` (EBR)**: Registered read epochs guarantee wait-free reads on cache hits while writers defer memory reclamation until readers progress.
- **`HazardPointers`**: Thread-level pointer publishing preventing writers from deallocating active nodes.
- **`ThreadLocalCacheFront`**: Thread-local ring-buffer front cache enabling local lookups with zero atomics and zero lock contention.
- **`SingleThreaded`**: Zero-cost policy for single-threaded or thread-isolated workloads.

### 5. Memory & Telemetry Subsystems
- **`NumaArenaResource`**: `std::pmr::memory_resource` compliant allocator pinning memory to specific NUMA sockets (or cache-line aligned arena on UMA).
- **`HugePageAllocator`**: Bypasses standard 4KB pages in favor of 2MB huge pages to eliminate TLB misses across massive address spaces.
- **`CacheLineVisualizer`**: Compile-time utility asserting and diagramming 64-byte L1 cache-line boundaries to prevent false sharing.
- **`MetaDumper`**: Zero-overhead JSON telemetry serializer introspecting operational stats and compile-time template parameters.

---

## Real-World Use Cases & Architectural Blueprints

Ginnunga is purposefully architected as a modular, concept-driven framework. By swapping storage backends, eviction policies, and concurrency models, engineers can tailor the cache to exact hardware and domain constraints:

### 1. High-Frequency Trading (HFT) & Exchange Connectivity
* **The Problem**: Electronic trading gateways (FIX, OUCH, ITCH) generate sparse 64-bit Client Order IDs (`ClOrdID`) distributed across multi-million keyspaces. However, only in-flight open orders (~thousands) require active routing. Standard hash maps induce nondeterministic hash collisions, pointer-chasing bucket traversals, and dynamic memory allocations on the critical trading path.
* **Architectural Blueprint**:
  ```cpp
  using FIXOrderCache = ginnunga::SparseCache<
      uint64_t,
      OrderState,
      ginnunga::storage::PagedSparseArray<uint64_t, OrderState>,
      ginnunga::eviction::TTLPolicy<uint64_t>,
      ginnunga::concurrency::SingleThreaded
  >;
  ```
* **Why It Excels**:
  * **Direct Virtual Indexing**: `PagedSparseArray` reserves a massive virtual memory window via `mmap`. Key lookups translate directly to memory offsets with **sub-10ns latency** and zero hashing overhead.
  * **Demand Paging & Lazy OS Commits**: Physical RAM is committed on-demand only for accessed order pages and recycled back to the kernel via `madvise(MADV_DONTNEED)`.
  * **Monotonic TTL Expiration**: High-precision monotonic timestamps seamlessly expire dead sessions and unacknowledged orders without polling threads.
  * **64-Byte Cache-Line Packing**: `OrderState` is aligned to 64 bytes (`alignas(64)`), eliminating false sharing across gateway cores.

---

### 2. Game Engines & Real-Time Simulation (Entity Component Systems)
* **The Problem**: Open-world engines and simulations manage millions of sparse entity IDs (`EntityID`), but specific gameplay components (e.g. `AIComponent`, `CombatState`, `PhysicsBody`) are only attached to a fraction of entities. Traversing hash maps or pointer arrays on every frame tick causes severe CPU cache thrashing and breaks SIMD vectorization.
* **Architectural Blueprint**:
  ```cpp
  using AIComponentCache = ginnunga::SparseCache<
      EntityID,
      AIComponent,
      ginnunga::storage::SlotArrayMap<EntityID, AIComponent>,
      ginnunga::eviction::LRUPolicy<EntityID>,
      ginnunga::concurrency::SingleThreaded
  >;
  ```
* **Why It Excels**:
  * **Sparse Set Invariant**: `SlotArrayMap` maintains two arrays: a sparse array for $O(1)$ random lookups by `EntityID`, and a packed dense array containing exclusively active components.
  * **Vectorized Cache-Line Streaming**: Game systems iterate directly over `dense_values()` in a single contiguous memory block. The CPU prefetcher streams data with **100% cache line utilization**, enabling compiler auto-vectorization (AVX-512 / NEON).
  * **$O(1)$ Swap-and-Pop Despawning**: Entity destruction swaps the target component with the last element of the dense array and pops it in constant time with zero memory fragmentation.

---

### 3. High-Throughput Networking & Flow Tracking (DPDK / eBPF / SDN)
* **The Problem**: Userspace packet processing engines (DPDK, XDP, eBPF) must track active TCP/UDP connections across an astronomical 144-bit IPv6/port 5-tuple space ($2^{144}$ addresses). Multi-threaded packet routing threads contend heavily on locks, while flash crowds or DDoS floods cause severe cache thrashing.
* **Architectural Blueprint**:
  ```cpp
  using FlowTracker = ginnunga::SparseCache<
      IPv6_Tuple,
      SessionState,
      ginnunga::storage::SimdFlatMap<IPv6_Tuple, SessionState, IPv6Hash>,
      ginnunga::eviction::ARCPolicy<IPv6_Tuple>,
      ginnunga::concurrency::EpochReclamation
  >;
  ```
* **Why It Excels**:
  * **16-Lane SIMD Vector Probing**: `SimdFlatMap` uses ARM NEON or AVX2 instructions to probe 16 metadata control bytes in parallel in a single CPU cycle, skipping empty slots and tombstones instantly.
  * **Wait-Free Reads via EBR**: Worker packet-processing threads read session records concurrently without acquiring mutexes or executing atomic write instructions (`EpochReclamation`). Writers defer memory reclamation until readers rotate epochs.
  * **Self-Tuning ARC Eviction**: The Adaptive Replacement Cache dynamically self-tunes its balance parameter ($p$) between recency ($T_1$) and frequency ($T_2$). It resists DDoS SYN-flood scans while retaining legitimate long-lived TCP streams.

---

### 4. Storage Engines & Log-Structured Merge (LSM) Block Caches
* **The Problem**: Flash/NVMe storage engines (e.g. RocksDB/LevelDB architectures) cache sparse 64-bit disk chunk/block identifiers. Keys exhibit strong prefix correlations. Traditional hash tables suffer from hash collision vulnerabilities, while multi-socket servers suffer severe NUMA interconnect penalties.
* **Architectural Blueprint**:
  ```cpp
  using BlockCache = ginnunga::SparseCache<
      uint64_t,
      BlockDescriptor,
      ginnunga::storage::JudyTreeBackend<uint64_t, BlockDescriptor>,
      ginnunga::eviction::LFUPolicy<uint64_t>,
      ginnunga::concurrency::SingleThreaded
  >;
  ```
* **Why It Excels**:
  * **Compressed Adaptive Radix Trie**: `JudyTreeBackend` compresses shared byte prefixes across 64-bit block keys, guaranteeing bounded, deterministic lookup depths with zero hash collision risks.
  * **NUMA Socket Pinning**: Can be coupled with `NumaArenaResource` to allocate cache nodes strictly on the local NUMA domain of the NVMe controller, eliminating cross-socket QPI/UPI bus traffic.
  * **2MB Huge Page TLB Elimination**: `HugePageAllocator` maps memory in 2MB pages, reducing TLB translation overhead across multi-gigabyte cache working sets.

---

### 5. Distributed Tracing & Real-Time APM Observability
* **The Problem**: Microservices and service meshes (Envoy, OpenTelemetry) emit millions of sparse 64-bit or 128-bit Trace IDs. Traces must be correlated within a sliding completion window and exported before memory is exhausted.
* **Architectural Blueprint**:
  ```cpp
  using TraceBuffer = ginnunga::SparseCache<
      TraceID,
      SpanContext,
      ginnunga::storage::SimdFlatMap<TraceID, SpanContext>,
      ginnunga::eviction::TTLPolicy<TraceID>,
      ginnunga::concurrency::ThreadLocalCacheFront<TraceID, SpanContext>
  >;
  ```
* **Why It Excels**:
  * **Thread-Local L1 Front Ring**: Consecutive intra-thread span updates are intercepted by `ThreadLocalCacheFront` in an uncontented L1 ring buffer without touching shared atomic variables.
  * **Sliding TTL Window**: Inactive or completed traces are automatically reaped after the sliding timeout window elapses.
  * **Zero-Overhead JSON Telemetry**: `MetaDumper` outputs real-time cache performance and reflection metrics directly into Prometheus or Grafana collector agents.

---

## Architecture Selection Matrix

| Workload / Requirement | Recommended Storage | Recommended Eviction | Recommended Concurrency | Memory Strategy |
| :--- | :--- | :--- | :--- | :--- |
| **Exchange Order Gateways (FIX/OUCH)** | `PagedSparseArray` | `TTLPolicy` | `SingleThreaded` | `NumaArenaResource` (64B align) |
| **Game Engine ECS Contiguous Ticks** | `SlotArrayMap` | `LRUPolicy` | `SingleThreaded` | Standard Heap / Arena |
| **High-Throughput IPv6 / Packet Routing** | `SimdFlatMap` | `ARCPolicy` | `EpochReclamation` | 64B Cache-Line Aligned |
| **Multi-Threaded Node-Level Publishing** | `SimdFlatMap` | `ARCPolicy` | `HazardPointers` | Standard Heap |
| **Prefix-Correlated Block & Chunk Caching** | `JudyTreeBackend` | `LFUPolicy` | `SingleThreaded` | `HugePageAllocator` (2MB) |
| **Thread-Isolated Ultra-Hot Loops** | Any Storage | Any Eviction | `ThreadLocalCacheFront` | Thread-Local L1 Ring |

---

## Directory Structure

```
ginnunga/
├── CMakeLists.txt                    # Modern CMake build system
├── README.md                         # Project documentation and architectural guide
├── LICENSE                           # GNU Affero General Public License v3.0
├── CREDITS.md                        # Algorithm citations and acknowledgments
├── assets/
│   └── logo.jpg                      # Project brand logo
├── include/ginnunga/
│   ├── cache.hpp                     # Master SparseCache orchestrator
│   ├── concepts/
│   │   ├── storage.hpp               # StorageConcept C++20 interface
│   │   ├── eviction.hpp              # EvictionConcept C++20 interface
│   │   ├── concurrency.hpp           # ConcurrencyConcept C++20 interface
│   │   └── memory.hpp                # MemoryResourceConcept C++20 interface
│   ├── memory/
│   │   ├── numa_arena_resource.hpp   # NUMA & 64B cache-line arena allocator
│   │   └── huge_page_allocator.hpp   # 2MB huge page mmap allocator
│   ├── storage/
│   │   ├── paged_sparse_array.hpp    # Virtual memory lazy-paged array (mmap)
│   │   ├── slot_array_map.hpp        # Sparse set with packed dense array
│   │   ├── simd_flat_map.hpp         # NEON/AVX2 SIMD control-byte hash map
│   │   └── judy_tree_backend.hpp     # Compressed adaptive radix trie
│   ├── eviction/
│   │   ├── lru_policy.hpp            # Least Recently Used (LRU) policy
│   │   ├── lfu_policy.hpp            # Least Frequently Used (LFU) policy
│   │   ├── arc_policy.hpp            # Adaptive Replacement Cache (ARC)
│   │   └── ttl_policy.hpp            # Time-To-Live monotonic clock policy
│   ├── concurrency/
│   │   ├── epoch_reclamation.hpp     # Epoch-Based Reclamation (EBR)
│   │   ├── hazard_pointers.hpp       # Hazard pointer publishing
│   │   ├── thread_local_cache_front.hpp # Thread-local L1 ring buffer
│   │   └── single_threaded.hpp       # Zero-cost single-threaded policy
│   └── telemetry/
│       ├── meta_dumper.hpp           # Zero-cost JSON telemetry serializer
│       └── cache_line_visualizer.hpp # 64-byte cache line layout inspector
├── src/
│   └── ginnunga.cpp                  # Library runtime symbols
├── examples/
│   ├── fix_order_cache.cpp           # Exchange Connectivity (PagedSparseArray + TTL)
│   ├── ecs_component_cache.cpp       # Game Engine ECS (SlotArrayMap + LRU)
│   └── tcp_session_tracker.cpp       # IPv6 Session Tracking (SimdFlatMap + ARC + EBR)
├── tests/
│   ├── test_framework.hpp            # Lightweight zero-dependency test harness
│   ├── test_main.cpp                 # Test runner entry point
│   ├── test_concepts.cpp             # Compile-time concept verification
│   ├── test_storage.cpp              # Storage backends unit tests & oracle stress tests
│   ├── test_eviction.cpp             # Eviction policies unit tests
│   ├── test_concurrency.cpp          # Concurrency models unit tests
│   ├── test_memory.cpp               # NUMA & huge page memory allocators tests
│   ├── test_telemetry.cpp            # Cache-line alignment & JSON telemetry tests
│   └── test_cache.cpp                # Integrated SparseCache tests
├── bench/
│   └── bench_cache.cpp               # High-throughput microbenchmarks
└── scripts/
    ├── build.sh                      # Compilation automation script
    ├── test.sh                       # Unit test and CTest runner script
    ├── run_examples.sh               # Application examples executor
    └── benchmark.sh                  # Microbenchmark runner
```

---

## Quick Start Example

```cpp
#include <iostream>
#include <ginnunga/cache.hpp>
#include <ginnunga/storage/simd_flat_map.hpp>
#include <ginnunga/eviction/arc_policy.hpp>
#include <ginnunga/concurrency/epoch_reclamation.hpp>

int main() {
    // Compose an ultra-low-latency cache with SIMD probing,
    // Adaptive Replacement Eviction, and Wait-Free EBR concurrency:
    using Cache = ginnunga::SparseCache<
        uint64_t,
        std::string,
        ginnunga::storage::SimdFlatMap<uint64_t, std::string>,
        ginnunga::eviction::ARCPolicy<uint64_t>,
        ginnunga::concurrency::EpochReclamation
    >;

    Cache cache(10000); // 10,000 capacity

    cache.put(42ULL, "OrderExecuted");
    
    if (auto val = cache.get(42ULL); val.has_value()) {
        std::cout << "Cached value: " << *val << "\n";
    }

    // Inspect live telemetry JSON
    std::cout << cache.dump_telemetry_json("ProductionCache") << "\n";
    return 0;
}
```

---

## Microbenchmarks & Performance Profiles

Microbenchmarks evaluated on Apple Silicon (M-series, ARM64 Darwin) across 500,000 operations comparing Ginnunga storage backends against `std::unordered_map`:

| Storage Architecture | Lookup Throughput | Insert Throughput | Memory Footprint (500K keys) | Latency Profile |
| :--- | :---: | :---: | :---: | :--- |
| **`PagedSparseArray`** (`mmap`) | **125.77 M ops/sec** | 14.06 M ops/sec | **1,264 KB** | Direct memory offset, zero hashing |
| **`SlotArrayMap`** (Sparse Set) | **98.41 M ops/sec** | **17.88 M ops/sec** | 4,930 KB | Packed contiguous array, vector-friendly |
| **`SimdFlatMap`** (NEON SIMD) | 9.66 M ops/sec | 3.90 M ops/sec | 63,476 KB | 16-lane parallel control-byte probing |
| **`std::unordered_map`** (Baseline) | 15.68 M ops/sec | 8.63 M ops/sec | 22,859 KB | High pointer-chasing overhead (18x RAM) |

> Run `./scripts/benchmark.sh` to replicate on your local hardware.

---

## Building and Testing

### Prerequisites
- Modern C++ compiler supporting C++20 or C++26 (Apple Clang 15+, Clang 16+, GCC 12+)
- CMake 3.22+

### Convenience Scripts
```bash
# Build the project (Release mode by default)
./scripts/build.sh

# Run the complete unit test suite and CTest
./scripts/test.sh

# Run all application examples (or pass: fix_order, ecs, tcp)
./scripts/run_examples.sh

# Run microbenchmarks
./scripts/benchmark.sh
```

### Manual CMake Build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Run Unit Tests
```bash
./build/ginnunga_tests
```

### Run Microbenchmarks
```bash
./build/ginnunga_bench
```

### Run Application Examples
```bash
# 1. Exchange Connectivity (FIX Order IDs with Virtual Memory Paging)
./build/example_fix_order

# 2. Game Engine ECS Component Cache (Sparse Sets with Cache-Line Density)
./build/example_ecs_component

# 3. Userspace IPv6 Session Tracker (SIMD Flat Map + ARC + EBR)
./build/example_tcp_session
```

---

## License
This project is licensed under the **GNU Affero General Public License v3.0 (AGPL-3.0)**. See the [LICENSE](file:///Users/zeeshanqazi/projects/Ginnunga/LICENSE) file for complete terms and conditions.

See [CREDITS.md](file:///Users/zeeshanqazi/projects/Ginnunga/CREDITS.md) for research citations, algorithmic inspirations, and acknowledgments.
