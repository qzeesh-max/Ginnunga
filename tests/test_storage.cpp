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

    storage.clear();
    ASSERT_EQ(storage.size(), 0);
}

TEST(Storage, SimdFlatMapOperations) {
    ginnunga::storage::SimdFlatMap<uint64_t, double> map(32);

    ASSERT_EQ(map.size(), 0);
    for (uint64_t i = 0; i < 50; ++i) {
        ASSERT_TRUE(map.insert(i * 1000ULL, static_cast<double>(i) * 1.5));
    }

    ASSERT_EQ(map.size(), 50);

    for (uint64_t i = 0; i < 50; ++i) {
        double* val = map.find(i * 1000ULL);
        ASSERT_TRUE(val != nullptr);
        ASSERT_EQ(*val, static_cast<double>(i) * 1.5);
    }

    // Non-existent key
    ASSERT_TRUE(map.find(999999ULL) == nullptr);

    // Erase
    ASSERT_TRUE(map.erase(0));
    ASSERT_EQ(map.size(), 49);
    ASSERT_TRUE(map.find(0) == nullptr);

    map.clear();
    ASSERT_EQ(map.size(), 0);
}

TEST(Storage, JudyTreeBackendOperations) {
    ginnunga::storage::JudyTreeBackend<uint32_t, int> trie;

    ASSERT_EQ(trie.size(), 0);
    ASSERT_TRUE(trie.insert(0x12345678, 42));
    ASSERT_TRUE(trie.insert(0x12345679, 43));
    ASSERT_TRUE(trie.insert(0x87654321, 99));
    ASSERT_EQ(trie.size(), 3);

    int* v1 = trie.find(0x12345678);
    ASSERT_TRUE(v1 != nullptr);
    ASSERT_EQ(*v1, 42);

    int* v2 = trie.find(0x12345679);
    ASSERT_TRUE(v2 != nullptr);
    ASSERT_EQ(*v2, 43);

    int* v3 = trie.find(0x87654321);
    ASSERT_TRUE(v3 != nullptr);
    ASSERT_EQ(*v3, 99);

    ASSERT_TRUE(trie.find(0x00000000) == nullptr);

    ASSERT_TRUE(trie.erase(0x12345678));
    ASSERT_EQ(trie.size(), 2);
    ASSERT_TRUE(trie.find(0x12345678) == nullptr);

    trie.clear();
    ASSERT_EQ(trie.size(), 0);
}
