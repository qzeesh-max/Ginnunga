#include "test_framework.hpp"
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
