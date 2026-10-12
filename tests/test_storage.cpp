#include "test_framework.hpp"
#include <random>
#include <unordered_map>
#include <map>
#include <vector>
#include <algorithm>
#include <string>
#include "ginnunga/storage/paged_sparse_array.hpp"
#include "ginnunga/storage/slot_array_map.hpp"
#include "ginnunga/storage/simd_flat_map.hpp"
#include "ginnunga/storage/judy_tree_backend.hpp"

TEST(Storage, PagedSparseArrayOperations) {
    ginnunga::storage::PagedSparseArray<uint32_t, int> storage(100000);

    ASSERT_EQ(storage.size(), 0);
    ASSERT_TRUE(storage.find(42) == nullptr);

    ASSERT_TRUE(storage.insert(42, 100));
    ASSERT_TRUE(storage.insert(99999, 200));
    ASSERT_EQ(storage.size(), 2);

    int* val1 = storage.find(42);
    ASSERT_TRUE(val1 != nullptr);
    ASSERT_EQ(*val1, 100);

    // Overwrite existing key
    ASSERT_TRUE(storage.insert(42, 150));
    ASSERT_EQ(storage.size(), 2);
    ASSERT_EQ(*storage.find(42), 150);

    ASSERT_TRUE(storage.memory_footprint() > 0);

    int* val2 = storage.find(99999);
    ASSERT_TRUE(val2 != nullptr);
    ASSERT_EQ(*val2, 200);

    // Erase
    ASSERT_TRUE(storage.erase(42));
    ASSERT_EQ(storage.size(), 1);
    ASSERT_TRUE(storage.find(42) == nullptr);

    // Clear
    storage.clear();
    ASSERT_EQ(storage.size(), 0);
    ASSERT_TRUE(storage.find(99999) == nullptr);
}

TEST(Storage, SlotArrayMapOperations) {
    ginnunga::storage::SlotArrayMap<uint32_t, std::string> storage(1024);

    ASSERT_EQ(storage.size(), 0);
    ASSERT_TRUE(storage.insert(10, "ComponentA"));
    ASSERT_TRUE(storage.insert(500, "ComponentB"));
    ASSERT_TRUE(storage.insert(1000, "ComponentC"));
    ASSERT_EQ(storage.size(), 3);

    ASSERT_EQ(*storage.find(10), "ComponentA");
    ASSERT_EQ(*storage.find(500), "ComponentB");
    ASSERT_EQ(*storage.find(1000), "ComponentC");

    // Dense iteration verification
    const auto& dense_keys = storage.dense_keys();
    ASSERT_EQ(dense_keys.size(), 3);
    ASSERT_EQ(dense_keys[0], 10);
    ASSERT_EQ(dense_keys[1], 500);
    ASSERT_EQ(dense_keys[2], 1000);

    // Erase with swap-and-pop
    ASSERT_TRUE(storage.erase(500));
    ASSERT_EQ(storage.size(), 2);
    ASSERT_TRUE(storage.find(500) == nullptr);
    ASSERT_EQ(*storage.find(1000), "ComponentC");

    // Dynamic sparse array expansion (key 4096 exceeds initial size of 1024)
    ASSERT_TRUE(storage.insert(4096, "ComponentLargeKey"));
    ASSERT_EQ(storage.size(), 3);
    ASSERT_EQ(*storage.find(4096), "ComponentLargeKey");

    // Look up non-existent key greater than sparse size and within sparse size
    ASSERT_TRUE(storage.find(99999) == nullptr);
    ASSERT_TRUE(storage.find(20) == nullptr);

    storage.clear();
    ASSERT_EQ(storage.size(), 0);
    ASSERT_TRUE(storage.find(4096) == nullptr);
}

TEST(Storage, SimdFlatMapOperations) {
    ginnunga::storage::SimdFlatMap<uint64_t, double> map(32);

    ASSERT_EQ(map.size(), 0);
    // Erase on empty map returns false
    ASSERT_FALSE(map.erase(123ULL));

    for (uint64_t i = 0; i < 50; ++i) {
        ASSERT_TRUE(map.insert(i * 1000ULL, static_cast<double>(i) * 1.5));
    }

    ASSERT_EQ(map.size(), 50);

    // Overwrite existing key
    ASSERT_TRUE(map.insert(0ULL, 99.99));
    ASSERT_EQ(map.size(), 50);
    ASSERT_EQ(*map.find(0ULL), 99.99);

    for (uint64_t i = 1; i < 50; ++i) {
        double* val = map.find(i * 1000ULL);
        ASSERT_TRUE(val != nullptr);
        ASSERT_EQ(*val, static_cast<double>(i) * 1.5);
    }

    // Non-existent key lookup and erase
    ASSERT_TRUE(map.find(999999ULL) == nullptr);
    ASSERT_FALSE(map.erase(999999ULL));

    ASSERT_TRUE(map.memory_footprint() > 0);

    // Erase
    ASSERT_TRUE(map.erase(0ULL));
    ASSERT_EQ(map.size(), 49);
    ASSERT_TRUE(map.find(0ULL) == nullptr);

    map.clear();
    ASSERT_EQ(map.size(), 0);
}

TEST(Storage, JudyTreeBackendOperations) {
    ginnunga::storage::JudyTreeBackend<uint32_t, int> trie;

    ASSERT_EQ(trie.size(), 0);
    // Erase on empty trie
    ASSERT_FALSE(trie.erase(0x12345678));

    ASSERT_TRUE(trie.insert(0x12345678, 42));
    ASSERT_TRUE(trie.insert(0x12345679, 43));
    ASSERT_TRUE(trie.insert(0x87654321, 99));
    ASSERT_EQ(trie.size(), 3);

    // Overwrite existing key
    ASSERT_TRUE(trie.insert(0x12345678, 555));
    ASSERT_EQ(trie.size(), 3);
    ASSERT_EQ(*trie.find(0x12345678), 555);

    int* v2 = trie.find(0x12345679);
    ASSERT_TRUE(v2 != nullptr);
    ASSERT_EQ(*v2, 43);

    int* v3 = trie.find(0x87654321);
    ASSERT_TRUE(v3 != nullptr);
    ASSERT_EQ(*v3, 99);

    // Look up non-existent keys
    ASSERT_TRUE(trie.find(0x00000000) == nullptr);
    ASSERT_TRUE(trie.find(0x1234567A) == nullptr); // Same prefix, different last byte

    // Erase non-existent keys
    ASSERT_FALSE(trie.erase(0x00000000));
    ASSERT_FALSE(trie.erase(0x1234567A));

    ASSERT_TRUE(trie.memory_footprint() > 0);

    // Erase one key
    ASSERT_TRUE(trie.erase(0x12345678));
    ASSERT_EQ(trie.size(), 2);
    ASSERT_TRUE(trie.find(0x12345678) == nullptr);

    // Erase sibling to trigger branch pruning
    ASSERT_TRUE(trie.erase(0x12345679));
    ASSERT_EQ(trie.size(), 1);
    ASSERT_TRUE(trie.find(0x87654321) != nullptr);

    trie.clear();
    ASSERT_EQ(trie.size(), 0);
}

TEST(Storage, PagedSparseArrayMoveAndExpansion) {
    ginnunga::storage::PagedSparseArray<uint32_t, int> a(1024);
    a.insert(10, 100);
    // Dynamic virtual expansion across large sparse jump
    a.insert(50000, 500);
    ASSERT_EQ(a.size(), 2);

    // Test move constructor
    ginnunga::storage::PagedSparseArray<uint32_t, int> b(std::move(a));
    ASSERT_EQ(b.size(), 2);
    ASSERT_EQ(*b.find(10), 100);
    ASSERT_EQ(*b.find(50000), 500);

    // Test move assignment
    ginnunga::storage::PagedSparseArray<uint32_t, int> c(10);
    c = std::move(b);
    ASSERT_EQ(c.size(), 2);
    ASSERT_EQ(*c.find(10), 100);
    ASSERT_EQ(*c.find(50000), 500);

    // Test erase non-existent key
    ASSERT_FALSE(c.erase(999999));
}

TEST(Storage, SlotArrayMapMutableDenseAndEdgeCases) {
    ginnunga::storage::SlotArrayMap<uint32_t, int> sam(100);
    sam.insert(1, 10);
    sam.insert(2, 20);
    sam.insert(3, 30);

    // Update existing key
    sam.insert(2, 25);
    ASSERT_EQ(sam.size(), 3);
    ASSERT_EQ(*sam.find(2), 25);

    // Mutable direct access
    auto& values = sam.dense_values();
    for (auto& v : values) {
        v *= 2;
    }
    ASSERT_EQ(*sam.find(1), 20);
    ASSERT_EQ(*sam.find(2), 50);
    ASSERT_EQ(*sam.find(3), 60);

    // Erase middle element
    ASSERT_TRUE(sam.erase(2));
    ASSERT_EQ(sam.size(), 2);
    ASSERT_TRUE(sam.find(2) == nullptr);

    // Erase last element
    ASSERT_TRUE(sam.erase(3));
    ASSERT_EQ(sam.size(), 1);

    // Erase remaining single element
    ASSERT_TRUE(sam.erase(1));
    ASSERT_EQ(sam.size(), 0);

    // Erase non-existent
    ASSERT_FALSE(sam.erase(1));
}

TEST(Storage, SimdFlatMapRehashAndTombstoneReuse) {
    ginnunga::storage::SimdFlatMap<uint64_t, int> map(16);

    // Insert 500 elements to force multiple growth rehashes
    for (uint64_t i = 1; i <= 500; ++i) {
        ASSERT_TRUE(map.insert(i, static_cast<int>(i * 10)));
    }
    ASSERT_EQ(map.size(), 500);

    for (uint64_t i = 1; i <= 500; ++i) {
        int* v = map.find(i);
        ASSERT_TRUE(v != nullptr);
        ASSERT_EQ(*v, static_cast<int>(i * 10));
    }

    // Erase 250 elements (creates tombstones)
    for (uint64_t i = 1; i <= 250; ++i) {
        ASSERT_TRUE(map.erase(i));
    }
    ASSERT_EQ(map.size(), 250);

    // Re-insert 250 elements into tombstones
    for (uint64_t i = 1; i <= 250; ++i) {
        ASSERT_TRUE(map.insert(i, static_cast<int>(i * 20)));
    }
    ASSERT_EQ(map.size(), 500);

    for (uint64_t i = 1; i <= 250; ++i) {
        ASSERT_EQ(*map.find(i), static_cast<int>(i * 20));
    }
}

TEST(Storage, JudyTreePrefixBranchesAndMove) {
    ginnunga::storage::JudyTreeBackend<uint32_t, int> trie;

    // Keys sharing 3-byte prefix (0xABCD0001, 0xABCD0002)
    trie.insert(0xABCD0001, 111);
    trie.insert(0xABCD0002, 222);
    trie.insert(0xABCD0101, 333);
    ASSERT_EQ(trie.size(), 3);

    // Update existing leaf
    trie.insert(0xABCD0001, 999);
    ASSERT_EQ(trie.size(), 3);
    ASSERT_EQ(*trie.find(0xABCD0001), 999);

    // Test move constructor
    auto moved_trie = std::move(trie);
    ASSERT_EQ(moved_trie.size(), 3);
    ASSERT_EQ(*moved_trie.find(0xABCD0001), 999);
    ASSERT_EQ(*moved_trie.find(0xABCD0002), 222);

    // Move assignment
    ginnunga::storage::JudyTreeBackend<uint32_t, int> assigned;
    assigned = std::move(moved_trie);
    ASSERT_EQ(assigned.size(), 3);
    ASSERT_EQ(*assigned.find(0xABCD0101), 333);
}

// ============================================================================
// Detailed Correctness Tests: PagedSparseArray
// ============================================================================

TEST(Storage, PagedSparseArrayBoundaryAndPagingCorrectness) {
    // Virtual capacity 2,000,000 slots
    ginnunga::storage::PagedSparseArray<uint32_t, int> storage(2000000);

    // Test extreme boundary keys and cross-page keys
    std::vector<uint32_t> boundary_keys = {
        0, 1, 4095, 4096, 4097, 16383, 16384, 16385, 65535, 65536,
        131071, 131072, 500000, 1000000, 1999999
    };

    for (std::size_t i = 0; i < boundary_keys.size(); ++i) {
        uint32_t key = boundary_keys[i];
        int val = static_cast<int>(i * 100 + 7);
        ASSERT_TRUE(storage.insert(key, val));
    }
    ASSERT_EQ(storage.size(), boundary_keys.size());

    // Verify all keys retrieve correct values
    for (std::size_t i = 0; i < boundary_keys.size(); ++i) {
        uint32_t key = boundary_keys[i];
        int expected = static_cast<int>(i * 100 + 7);
        int* found = storage.find(key);
        ASSERT_TRUE(found != nullptr);
        ASSERT_EQ(*found, expected);
    }

    // Verify neighbors of boundary keys are null
    ASSERT_TRUE(storage.find(2) == nullptr);
    ASSERT_TRUE(storage.find(4094) == nullptr);
    ASSERT_TRUE(storage.find(16382) == nullptr);
    ASSERT_TRUE(storage.find(65537) == nullptr);

    // Overwrite some boundary keys with new values
    ASSERT_TRUE(storage.insert(0, 99999));
    ASSERT_EQ(*storage.find(0), 99999);
    ASSERT_TRUE(storage.insert(16384, 88888));
    ASSERT_EQ(*storage.find(16384), 88888);
    ASSERT_EQ(storage.size(), boundary_keys.size());

    // Erase keys and verify slot presence is properly cleared
    ASSERT_TRUE(storage.erase(0));
    ASSERT_TRUE(storage.find(0) == nullptr);
    ASSERT_FALSE(storage.erase(0)); // Second erase fails
    ASSERT_EQ(storage.size(), boundary_keys.size() - 1);

    ASSERT_TRUE(storage.erase(16384));
    ASSERT_TRUE(storage.find(16384) == nullptr);
    ASSERT_EQ(storage.size(), boundary_keys.size() - 2);

    // Re-insert into erased slots
    ASSERT_TRUE(storage.insert(0, 11111));
    ASSERT_EQ(*storage.find(0), 11111);
    ASSERT_TRUE(storage.insert(16384, 22222));
    ASSERT_EQ(*storage.find(16384), 22222);
    ASSERT_EQ(storage.size(), boundary_keys.size());

    // Dynamic virtual expansion beyond initial virtual capacity
    uint32_t huge_key = 3500000; // > 2,000,000
    ASSERT_TRUE(storage.insert(huge_key, 777777));
    ASSERT_TRUE(storage.find(huge_key) != nullptr);
    ASSERT_EQ(*storage.find(huge_key), 777777);
    ASSERT_EQ(storage.size(), boundary_keys.size() + 1);
}

TEST(Storage, PagedSparseArrayRandomizedOracleStressTest) {
    ginnunga::storage::PagedSparseArray<uint32_t, int> storage(50000);
    std::unordered_map<uint32_t, int> oracle;

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<uint32_t> key_dist(0, 150000);
    std::uniform_int_distribution<int> val_dist(1, 1000000);
    std::uniform_int_distribution<int> op_dist(0, 9);

    for (int step = 0; step < 2500; ++step) {
        int op = op_dist(rng);
        uint32_t key = key_dist(rng);

        if (op < 5) {
            // Insert / update
            int val = val_dist(rng);
            storage.insert(key, val);
            oracle[key] = val;
        } else if (op < 8) {
            // Find query
            int* found = storage.find(key);
            auto it = oracle.find(key);
            if (it != oracle.end()) {
                ASSERT_TRUE(found != nullptr);
                ASSERT_EQ(*found, it->second);
            } else {
                ASSERT_TRUE(found == nullptr);
            }
        } else {
            // Erase
            bool res = storage.erase(key);
            bool expected = (oracle.erase(key) > 0);
            ASSERT_EQ(res, expected);
        }
    }

    ASSERT_EQ(storage.size(), oracle.size());
    for (const auto& [k, v] : oracle) {
        int* p = storage.find(k);
        ASSERT_TRUE(p != nullptr);
        ASSERT_EQ(*p, v);
    }
}

// ============================================================================
// Detailed Correctness Tests: SlotArrayMap
// ============================================================================

TEST(Storage, SlotArrayMapSparseSetInvariantPreservation) {
    ginnunga::storage::SlotArrayMap<uint32_t, int> sam(64);

    auto assert_invariants = [&sam]() {
        const auto& keys = sam.dense_keys();
        const auto& vals = sam.dense_values();
        ASSERT_EQ(keys.size(), sam.size());
        ASSERT_EQ(vals.size(), sam.size());
        for (std::size_t i = 0; i < sam.size(); ++i) {
            uint32_t k = keys[i];
            int* p = sam.find(k);
            ASSERT_TRUE(p != nullptr);
            ASSERT_EQ(p, &vals[i]);
            ASSERT_EQ(*p, vals[i]);
        }
    };

    // Step 1: Sequential insertions
    sam.insert(10, 100);
    sam.insert(20, 200);
    sam.insert(30, 300);
    sam.insert(40, 400);
    sam.insert(50, 500);
    assert_invariants();
    ASSERT_EQ(sam.size(), 5);

    // Step 2: Erase first element (10) -> last element (50) swapped to index 0
    ASSERT_TRUE(sam.erase(10));
    assert_invariants();
    ASSERT_EQ(sam.size(), 4);
    ASSERT_EQ(sam.dense_keys()[0], 50);
    ASSERT_EQ(*sam.find(50), 500);

    // Step 3: Erase middle element (30) -> last element (40) swapped into middle
    ASSERT_TRUE(sam.erase(30));
    assert_invariants();
    ASSERT_EQ(sam.size(), 3);
    ASSERT_EQ(*sam.find(40), 400);

    // Step 4: Erase last element (40) -> popped back directly
    ASSERT_TRUE(sam.erase(40));
    assert_invariants();
    ASSERT_EQ(sam.size(), 2);

    // Step 5: Duplicate insert must update value without changing dense array size
    sam.insert(20, 222);
    assert_invariants();
    ASSERT_EQ(sam.size(), 2);
    ASSERT_EQ(*sam.find(20), 222);

    // Step 6: Insert huge sparse key triggering dynamic sparse array resize
    sam.insert(30000, 33333);
    assert_invariants();
    ASSERT_EQ(sam.size(), 3);
    ASSERT_EQ(*sam.find(30000), 33333);

    // Step 7: Erase all remaining elements down to empty
    ASSERT_TRUE(sam.erase(50));
    assert_invariants();
    ASSERT_TRUE(sam.erase(20));
    assert_invariants();
    ASSERT_TRUE(sam.erase(30000));
    assert_invariants();
    ASSERT_EQ(sam.size(), 0);
    ASSERT_FALSE(sam.erase(999));
}

TEST(Storage, SlotArrayMapRandomizedOracleStressTest) {
    ginnunga::storage::SlotArrayMap<uint32_t, int> sam(128);
    std::unordered_map<uint32_t, int> oracle;

    std::mt19937_64 rng(12345);
    std::uniform_int_distribution<uint32_t> key_dist(0, 30000);
    std::uniform_int_distribution<int> val_dist(1, 100000);
    std::uniform_int_distribution<int> op_dist(0, 9);

    for (int step = 0; step < 2500; ++step) {
        int op = op_dist(rng);
        uint32_t key = key_dist(rng);

        if (op < 5) {
            int val = val_dist(rng);
            sam.insert(key, val);
            oracle[key] = val;
        } else if (op < 8) {
            int* found = sam.find(key);
            auto it = oracle.find(key);
            if (it != oracle.end()) {
                ASSERT_TRUE(found != nullptr);
                ASSERT_EQ(*found, it->second);
            } else {
                ASSERT_TRUE(found == nullptr);
            }
        } else {
            bool res = sam.erase(key);
            bool expected = (oracle.erase(key) > 0);
            ASSERT_EQ(res, expected);
        }

        if (step % 500 == 0) {
            ASSERT_EQ(sam.size(), oracle.size());
            ASSERT_EQ(sam.dense_keys().size(), oracle.size());
            ASSERT_EQ(sam.dense_values().size(), oracle.size());
        }
    }

    ASSERT_EQ(sam.size(), oracle.size());
    for (const auto& [k, v] : oracle) {
        int* p = sam.find(k);
        ASSERT_TRUE(p != nullptr);
        ASSERT_EQ(*p, v);
    }
}

// ============================================================================
// Detailed Correctness Tests: SimdFlatMap
// ============================================================================

struct ForcedCollisionHash {
    std::size_t operator()(uint32_t /*key*/) const noexcept {
        // Returns constant 0x42: same h2 (0x42) and same group index (0)
        return 0x42;
    }
};

TEST(Storage, SimdFlatMapHashCollisionAndProbingCorrectness) {
    // Capacity 64 (4 groups of 16 slots)
    ginnunga::storage::SimdFlatMap<uint32_t, int, ForcedCollisionHash> map(64);

    // Insert 24 keys with identical hash - forces group 0 to fill (16 lanes)
    // and spill over into group 1 (8 lanes)
    for (uint32_t i = 1; i <= 24; ++i) {
        ASSERT_TRUE(map.insert(i, static_cast<int>(i * 10)));
    }
    ASSERT_EQ(map.size(), 24);

    // Verify all 24 keys are found correctly despite hash collisions
    for (uint32_t i = 1; i <= 24; ++i) {
        int* val = map.find(i);
        ASSERT_TRUE(val != nullptr);
        ASSERT_EQ(*val, static_cast<int>(i * 10));
    }

    // Look up non-existent key: must scan across both colliding groups and return nullptr
    ASSERT_TRUE(map.find(999) == nullptr);

    // Overwrite existing key in colliding group
    ASSERT_TRUE(map.insert(5, 5555));
    ASSERT_EQ(*map.find(5), 5555);
    ASSERT_EQ(map.size(), 24);

    // Erase key from first group (e.g. key 1)
    ASSERT_TRUE(map.erase(1));
    ASSERT_EQ(map.size(), 23);
    ASSERT_TRUE(map.find(1) == nullptr);

    // Keys that collided and spilled into group 1 must still be found!
    for (uint32_t i = 2; i <= 24; ++i) {
        int* val = map.find(i);
        ASSERT_TRUE(val != nullptr);
        if (i == 5) {
            ASSERT_EQ(*val, 5555);
        } else {
            ASSERT_EQ(*val, static_cast<int>(i * 10));
        }
    }

    // Erase all remaining keys one by one
    for (uint32_t i = 2; i <= 24; ++i) {
        ASSERT_TRUE(map.erase(i));
    }
    ASSERT_EQ(map.size(), 0);
    ASSERT_TRUE(map.find(10) == nullptr);
}

TEST(Storage, SimdFlatMapTombstoneChainAndGrowthStressTest) {
    ginnunga::storage::SimdFlatMap<uint64_t, int> map(32);
    std::unordered_map<uint64_t, int> oracle;

    // Phase 1: Create dense alternating tombstone patterns
    for (uint64_t i = 1; i <= 100; ++i) {
        map.insert(i, static_cast<int>(i * 10));
        oracle[i] = static_cast<int>(i * 10);
    }
    // Erase every even key -> creates 50 tombstones
    for (uint64_t i = 2; i <= 100; i += 2) {
        ASSERT_TRUE(map.erase(i));
        oracle.erase(i);
    }
    ASSERT_EQ(map.size(), 50);

    // Verify all odd keys are still found across the tombstone chains
    for (uint64_t i = 1; i <= 100; i += 2) {
        int* p = map.find(i);
        ASSERT_TRUE(p != nullptr);
        ASSERT_EQ(*p, static_cast<int>(i * 10));
    }

    // Re-insert 50 keys to reuse tombstones
    for (uint64_t i = 2; i <= 100; i += 2) {
        ASSERT_TRUE(map.insert(i, static_cast<int>(i * 20)));
        oracle[i] = static_cast<int>(i * 20);
    }
    ASSERT_EQ(map.size(), 100);

    // Phase 2: Randomized Stress vs Oracle
    std::mt19937_64 rng(98765);
    std::uniform_int_distribution<uint64_t> key_dist(1, 10000);
    std::uniform_int_distribution<int> val_dist(1, 500000);
    std::uniform_int_distribution<int> op_dist(0, 9);

    for (int step = 0; step < 3000; ++step) {
        int op = op_dist(rng);
        uint64_t key = key_dist(rng);

        if (op < 5) {
            int val = val_dist(rng);
            map.insert(key, val);
            oracle[key] = val;
        } else if (op < 8) {
            int* found = map.find(key);
            auto it = oracle.find(key);
            if (it != oracle.end()) {
                ASSERT_TRUE(found != nullptr);
                ASSERT_EQ(*found, it->second);
            } else {
                ASSERT_TRUE(found == nullptr);
            }
        } else {
            bool res = map.erase(key);
            bool expected = (oracle.erase(key) > 0);
            ASSERT_EQ(res, expected);
        }
    }

    ASSERT_EQ(map.size(), oracle.size());
    for (const auto& [k, v] : oracle) {
        int* p = map.find(k);
        ASSERT_TRUE(p != nullptr);
        ASSERT_EQ(*p, v);
    }
}

// ============================================================================
// Detailed Correctness Tests: JudyTreeBackend
// ============================================================================

TEST(Storage, JudyTreeRadixPrefixBranchingAnd64BitCorrectness) {
    // 32-bit prefix branching tests
    ginnunga::storage::JudyTreeBackend<uint32_t, int> trie32;

    // Branch A: shares 3 bytes (0xAA1122xx)
    trie32.insert(0xAA112201, 101);
    trie32.insert(0xAA112202, 102);
    // Branch B: shares 2 bytes (0xAA11xx00)
    trie32.insert(0xAA113301, 201);
    // Branch C: shares 1 byte (0xAAxxxxxx)
    trie32.insert(0xAA220001, 301);
    // Branch D: different root byte
    trie32.insert(0xBB000001, 401);
    trie32.insert(0x00000000, 501);
    trie32.insert(0xFFFFFFFF, 601);

    ASSERT_EQ(trie32.size(), 7);
    ASSERT_EQ(*trie32.find(0xAA112201), 101);
    ASSERT_EQ(*trie32.find(0xAA112202), 102);
    ASSERT_EQ(*trie32.find(0xAA113301), 201);
    ASSERT_EQ(*trie32.find(0xAA220001), 301);
    ASSERT_EQ(*trie32.find(0xBB000001), 401);
    ASSERT_EQ(*trie32.find(0x00000000), 501);
    ASSERT_EQ(*trie32.find(0xFFFFFFFF), 601);

    // Overwrite leaf without affecting siblings
    ASSERT_TRUE(trie32.insert(0xAA112201, 9999));
    ASSERT_EQ(*trie32.find(0xAA112201), 9999);
    ASSERT_EQ(*trie32.find(0xAA112202), 102);
    ASSERT_EQ(trie32.size(), 7);

    // 64-bit key radix tree test (8-byte trie depth)
    ginnunga::storage::JudyTreeBackend<uint64_t, std::string> trie64;

    // Keys sharing 7 bytes: 0x01020304050607xx
    trie64.insert(0x0102030405060701ULL, "Leaf1");
    trie64.insert(0x0102030405060702ULL, "Leaf2");
    trie64.insert(0x0102030405060801ULL, "Leaf3");
    trie64.insert(0xFEDCBA9876543210ULL, "HighLeaf");
    trie64.insert(0x0000000000000000ULL, "ZeroLeaf");

    ASSERT_EQ(trie64.size(), 5);
    ASSERT_EQ(*trie64.find(0x0102030405060701ULL), "Leaf1");
    ASSERT_EQ(*trie64.find(0x0102030405060702ULL), "Leaf2");
    ASSERT_EQ(*trie64.find(0x0102030405060801ULL), "Leaf3");
    ASSERT_EQ(*trie64.find(0xFEDCBA9876543210ULL), "HighLeaf");
    ASSERT_EQ(*trie64.find(0x0000000000000000ULL), "ZeroLeaf");

    ASSERT_TRUE(trie64.erase(0x0102030405060701ULL));
    ASSERT_EQ(trie64.size(), 4);
    ASSERT_TRUE(trie64.find(0x0102030405060701ULL) == nullptr);
    ASSERT_EQ(*trie64.find(0x0102030405060702ULL), "Leaf2");
}

TEST(Storage, JudyTreeRecursivePruningAndOracleStressTest) {
    ginnunga::storage::JudyTreeBackend<uint32_t, int> trie;

    // Insert cluster of 4 sibling leaves under single branch
    trie.insert(0x55443301, 1);
    trie.insert(0x55443302, 2);
    trie.insert(0x55443303, 3);
    trie.insert(0x55443304, 4);
    std::size_t mem_before = trie.memory_footprint();

    // Erasing all leaves under 0x554433xx should prune the branch
    ASSERT_TRUE(trie.erase(0x55443301));
    ASSERT_TRUE(trie.erase(0x55443302));
    ASSERT_TRUE(trie.erase(0x55443303));
    ASSERT_TRUE(trie.erase(0x55443304));
    ASSERT_EQ(trie.size(), 0);
    ASSERT_TRUE(trie.memory_footprint() < mem_before);

    // Randomized Stress Test vs Oracle
    std::unordered_map<uint32_t, int> oracle;
    std::mt19937_64 rng(54321);
    std::uniform_int_distribution<uint32_t> key_dist(0, 50000);
    std::uniform_int_distribution<int> val_dist(1, 1000000);
    std::uniform_int_distribution<int> op_dist(0, 9);

    for (int step = 0; step < 2500; ++step) {
        int op = op_dist(rng);
        uint32_t key = key_dist(rng);

        if (op < 5) {
            int val = val_dist(rng);
            trie.insert(key, val);
            oracle[key] = val;
        } else if (op < 8) {
            int* found = trie.find(key);
            auto it = oracle.find(key);
            if (it != oracle.end()) {
                ASSERT_TRUE(found != nullptr);
                ASSERT_EQ(*found, it->second);
            } else {
                ASSERT_TRUE(found == nullptr);
            }
        } else {
            bool res = trie.erase(key);
            bool expected = (oracle.erase(key) > 0);
            ASSERT_EQ(res, expected);
        }
    }

    ASSERT_EQ(trie.size(), oracle.size());
    for (const auto& [k, v] : oracle) {
        int* p = trie.find(k);
        ASSERT_TRUE(p != nullptr);
        ASSERT_EQ(*p, v);
    }
}

