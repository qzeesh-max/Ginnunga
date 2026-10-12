#include "test_framework.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include "ginnunga/concurrency/single_threaded.hpp"
#include "ginnunga/concurrency/epoch_reclamation.hpp"
#include "ginnunga/concurrency/hazard_pointers.hpp"
#include "ginnunga/concurrency/thread_local_cache_front.hpp"

TEST(Concurrency, SingleThreaded) {
    ginnunga::concurrency::SingleThreaded st;
    st.acquire_read();
    st.release_read();

    bool cleaned = false;
    st.retire([&cleaned]() { cleaned = true; });
    ASSERT_TRUE(cleaned);
    st.synchronize();
}

TEST(Concurrency, EpochReclamation) {
    ginnunga::concurrency::EpochReclamation ebr;

    std::atomic<bool> retired_called{false};

    // Thread 1 acquires read
    auto epoch1 = ebr.acquire_read();
    (void)epoch1;

    // Retire an action
    ebr.retire([&retired_called]() {
        retired_called.store(true);
    });

    ebr.release_read();
    ebr.synchronize();

    ASSERT_TRUE(retired_called.load());
}

TEST(Concurrency, HazardPointers) {
    ginnunga::concurrency::HazardPointers hp;

    int sample_node = 42;
    std::size_t slot = hp.acquire_read();
    hp.publish_hazard(slot, &sample_node);

    ASSERT_TRUE(hp.is_hazardous(&sample_node));
    ASSERT_FALSE(hp.is_hazardous(nullptr));

    hp.clear_hazard(slot);
    ASSERT_FALSE(hp.is_hazardous(&sample_node));

    hp.release_read();
    hp.synchronize();
}

TEST(Concurrency, ThreadLocalCacheFront) {
    ginnunga::concurrency::ThreadLocalCacheFront<int, std::string> front;

    ASSERT_TRUE(front.local_lookup(10) == nullptr);

    front.local_insert(10, "LocalValue");
    std::string* val = front.local_lookup(10);
    ASSERT_TRUE(val != nullptr);
    ASSERT_EQ(*val, "LocalValue");

    front.local_invalidate(10);
    ASSERT_TRUE(front.local_lookup(10) == nullptr);
}
