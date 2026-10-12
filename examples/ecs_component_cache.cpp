#include <iostream>
#include <vector>
#include <cstdint>
#include "ginnunga/cache.hpp"
#include "ginnunga/storage/slot_array_map.hpp"
#include "ginnunga/eviction/lru_policy.hpp"
#include "ginnunga/concurrency/single_threaded.hpp"
#include "ginnunga/telemetry/cache_line_visualizer.hpp"

using EntityID = uint32_t;

struct alignas(32) AIComponent {
    float behavior_tree_state{1.0f};
    float target_pos_x{0.0f};
    float target_pos_y{0.0f};
    float target_pos_z{0.0f};
    uint32_t aggro_level{5};
    uint32_t perception_radius{100};
};

int main() {
    std::cout << "========================================================\n";
    std::cout << "  Project Ginnunga - Game Engine ECS Component Cache\n";
    std::cout << "========================================================\n";

    std::cout << ginnunga::telemetry::CacheLineVisualizer<AIComponent>::describe_layout("AIComponent") << "\n";

    using ECSCache = ginnunga::SparseCache<
        EntityID,
        AIComponent,
        ginnunga::storage::SlotArrayMap<EntityID, AIComponent>,
        ginnunga::eviction::LRUPolicy<EntityID>,
        ginnunga::concurrency::SingleThreaded
    >;

    ECSCache ai_cache(1000);

    // Entities in a massive world (e.g. 1,000,000 entity IDs possible, but only few have AIComponent)
    std::vector<EntityID> ai_entities = { 12, 954, 4421, 55000, 120500, 899120 };

    std::cout << "[*] Spawning scattered game entities with AIComponent...\n";
    for (EntityID id : ai_entities) {
        AIComponent comp{};
        comp.target_pos_x = static_cast<float>(id % 100);
        comp.target_pos_y = static_cast<float>(id % 200);
        comp.target_pos_z = 10.0f;
        comp.aggro_level = (id % 10) + 1;

        ai_cache.put(id, comp);
        std::cout << "  -> Attached AIComponent to Entity " << id << "\n";
    }

    std::cout << "\n[*] Cache-line optimized contiguous iteration over active AI components:\n";
    const auto& dense_keys = ai_cache.storage().dense_keys();
    auto& dense_values = ai_cache.storage().dense_values();

    for (std::size_t i = 0; i < dense_keys.size(); ++i) {
        // Direct contiguous memory traversal - no pointer chasing, perfectly vectorizable
        dense_values[i].target_pos_x += 1.5f; // simulate frame tick update
        std::cout << "  [Tick] Entity " << dense_keys[i]
                  << " updated TargetPos X: " << dense_values[i].target_pos_x
                  << " | Aggro: " << dense_values[i].aggro_level << "\n";
    }

    std::cout << "\n[*] O(1) Sparse Query for Entity 55000:\n";
    if (auto* comp = ai_cache.find(55000)) {
        std::cout << "  [HIT] Entity 55000 found. Perception Radius: "
                  << comp->perception_radius << " units.\n";
    }

    std::cout << "\n[*] Despawning Entity 4421 (Swap-and-Pop in dense array):\n";
    ai_cache.erase(4421);
    std::cout << "  Active AI components remaining: " << ai_cache.size() << "\n";

    std::cout << "\n[*] Telemetry JSON Dump:\n";
    std::cout << ai_cache.dump_telemetry_json("ECS_AI_Component_Cache") << "\n";

    return 0;
}
