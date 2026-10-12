// SPDX-License-Identifier: AGPL-3.0-or-later
// Project Ginnunga: High-Performance C++ Sparse Cache Algorithm Framework
// Copyright (C) 2026 Zeeshan Qazi
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include "ginnunga/cache.hpp"
#include "ginnunga/storage/paged_sparse_array.hpp"
#include "ginnunga/eviction/ttl_policy.hpp"
#include "ginnunga/concurrency/single_threaded.hpp"
#include "ginnunga/telemetry/cache_line_visualizer.hpp"

// FIX Order State payload (cache-line optimized)
struct alignas(64) OrderState {
    char cl_ord_id[32]{};
    double price{0.0};
    uint32_t qty{0};
    char side{'1'}; // '1' = Buy, '2' = Sell
    char status{'0'}; // '0' = New, '1' = Partially Filled, '2' = Filled
};

int main() {
    std::cout << "========================================================\n";
    std::cout << "  Project Ginnunga - Exchange FIX Order Routing Cache\n";
    std::cout << "========================================================\n";

    // Visualize cache-line alignment of OrderState
    std::cout << ginnunga::telemetry::CacheLineVisualizer<OrderState>::describe_layout("OrderState") << "\n";

    using OrderCache = ginnunga::SparseCache<
        uint64_t,
        OrderState,
        ginnunga::storage::PagedSparseArray<uint64_t, OrderState>,
        ginnunga::eviction::TTLPolicy<uint64_t>,
        ginnunga::concurrency::SingleThreaded
    >;

    // 100,000 max capacity in active cache, virtual capacity in PagedSparseArray up to 10M keys
    OrderCache cache(100000);
    cache.eviction().set_ttl(std::chrono::milliseconds(200)); // 200ms TTL for demonstrative order expiration

    // Fragmented FIX Order IDs spread sparsely across a multi-million keyspace
    std::vector<uint64_t> fix_order_ids = {
        1000024ULL,
        2500091ULL,
        8700012ULL,
        9999999ULL
    };

    std::cout << "[*] Inserting active in-flight FIX orders into sparse virtual space...\n";
    for (uint64_t id : fix_order_ids) {
        OrderState order{};
        snprintf(order.cl_ord_id, sizeof(order.cl_ord_id), "ORD-FIX-%llu", id);
        order.price = 142.50 + static_cast<double>(id % 100) * 0.01;
        order.qty = 500;
        order.side = (id % 2 == 0) ? '1' : '2';
        order.status = '0';

        cache.put(id, order);
        std::cout << "  -> Inserted Order ID " << id << " (" << order.cl_ord_id << ") at $" << order.price << "\n";
    }

    std::cout << "\n[*] Cache Metrics:\n";
    std::cout << "  Active Entries:    " << cache.size() << "\n";
    std::cout << "  Memory Footprint:  " << cache.memory_footprint() / 1024 << " KB (Lazy OS pages committed)\n";

    std::cout << "\n[*] Performing O(1) Lookups (Zero pointer chasing):\n";
    for (uint64_t id : fix_order_ids) {
        if (auto opt = cache.get(id); opt.has_value()) {
            std::cout << "  [HIT] Found Order ID " << id << ": " << opt->cl_ord_id
                      << " | Price: $" << opt->price << " | Qty: " << opt->qty << "\n";
        }
    }

    std::cout << "\n[*] Simulating time passing beyond TTL (waiting 250ms)...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    std::cout << "[*] Checking order freshness (TTL expiration):\n";
    for (uint64_t id : fix_order_ids) {
        bool expired = cache.eviction().is_expired(id);
        std::cout << "  Order ID " << id << " expired? " << (expired ? "YES (Session closed)" : "NO") << "\n";
    }

    std::cout << "\n[*] Telemetry JSON Dump:\n";
    std::cout << cache.dump_telemetry_json("FIX_Order_Routing_Cache") << "\n";

    return 0;
}
