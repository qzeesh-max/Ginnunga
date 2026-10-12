#include "test_framework.hpp"
#include "ginnunga/concepts/storage.hpp"
#include "ginnunga/concepts/eviction.hpp"
#include "ginnunga/concepts/concurrency.hpp"
#include "ginnunga/concepts/memory.hpp"

#include "ginnunga/storage/paged_sparse_array.hpp"
#include "ginnunga/storage/slot_array_map.hpp"
#include "ginnunga/storage/simd_flat_map.hpp"
#include "ginnunga/storage/judy_tree_backend.hpp"

#include "ginnunga/eviction/lru_policy.hpp"
#include "ginnunga/eviction/lfu_policy.hpp"
#include "ginnunga/eviction/arc_policy.hpp"
#include "ginnunga/eviction/ttl_policy.hpp"

#include "ginnunga/concurrency/single_threaded.hpp"
#include "ginnunga/concurrency/epoch_reclamation.hpp"
#include "ginnunga/concurrency/hazard_pointers.hpp"
#include "ginnunga/concurrency/thread_local_cache_front.hpp"

TEST(Concepts, CompileTimeVerification) {
    // Storage concept assertions
    static_assert(ginnunga::concepts::StorageConcept<ginnunga::storage::PagedSparseArray<uint32_t, int>, uint32_t, int>);
    static_assert(ginnunga::concepts::StorageConcept<ginnunga::storage::SlotArrayMap<uint32_t, int>, uint32_t, int>);
    static_assert(ginnunga::concepts::StorageConcept<ginnunga::storage::SimdFlatMap<uint64_t, int>, uint64_t, int>);
    static_assert(ginnunga::concepts::StorageConcept<ginnunga::storage::JudyTreeBackend<uint32_t, int>, uint32_t, int>);

    // Eviction concept assertions
    static_assert(ginnunga::concepts::EvictionConcept<ginnunga::eviction::LRUPolicy<int>, int>);
    static_assert(ginnunga::concepts::EvictionConcept<ginnunga::eviction::LFUPolicy<int>, int>);
    static_assert(ginnunga::concepts::EvictionConcept<ginnunga::eviction::ARCPolicy<int>, int>);
    static_assert(ginnunga::concepts::EvictionConcept<ginnunga::eviction::TTLPolicy<int>, int>);

    // Concurrency concept assertions
    static_assert(ginnunga::concepts::ConcurrencyConcept<ginnunga::concurrency::SingleThreaded>);
    static_assert(ginnunga::concepts::ConcurrencyConcept<ginnunga::concurrency::EpochReclamation>);
    static_assert(ginnunga::concepts::ConcurrencyConcept<ginnunga::concurrency::HazardPointers>);
    static_assert(ginnunga::concepts::ConcurrencyConcept<ginnunga::concurrency::ThreadLocalCacheFront<int, int>>);

    ASSERT_TRUE(true);
}
