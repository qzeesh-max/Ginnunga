// SPDX-License-Identifier: AGPL-3.0-or-later
// Project Ginnunga: High-Performance C++ Sparse Cache Algorithm Framework
// Copyright (C) 2026 Zeeshan Qazi <zeeshan@zeeshan.im>
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
#include <array>
#include <thread>
#include <vector>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include "ginnunga/cache.hpp"
#include "ginnunga/storage/simd_flat_map.hpp"
#include "ginnunga/eviction/arc_policy.hpp"
#include "ginnunga/concurrency/epoch_reclamation.hpp"
#include "ginnunga/telemetry/cache_line_visualizer.hpp"

// 128-bit IPv6 address + 16-bit Port tuple
struct IPv6_Port_Pair {
    std::array<uint8_t, 16> ip{};
    uint16_t port{0};

    bool operator==(const IPv6_Port_Pair& other) const noexcept {
        return ip == other.ip && port == other.port;
    }
};

// Hash for IPv6_Port_Pair using FNV-1a
struct IPv6Hash {
    std::size_t operator()(const IPv6_Port_Pair& ep) const noexcept {
        std::size_t hash = 14695981039346656037ULL;
        for (uint8_t b : ep.ip) {
            hash ^= b;
            hash *= 1099511628211ULL;
        }
        hash ^= ep.port;
        hash *= 1099511628211ULL;
        return hash;
    }
};

template <>
struct std::hash<IPv6_Port_Pair> {
    std::size_t operator()(const IPv6_Port_Pair& ep) const noexcept {
        return IPv6Hash{}(ep);
    }
};

struct alignas(64) SessionState {
    uint32_t state{1}; // 1 = ESTABLISHED, 2 = TIME_WAIT, 3 = CLOSED
    uint64_t packets_forwarded{0};
    uint64_t bytes_transferred{0};
    uint32_t seq_num{1000};
    uint32_t ack_num{2000};
};

int main() {
    std::cout << "========================================================\n";
    std::cout << "  Project Ginnunga - Userspace IPv6 TCP Session Tracker\n";
    std::cout << "========================================================\n";

    std::cout << ginnunga::telemetry::CacheLineVisualizer<SessionState>::describe_layout("SessionState") << "\n";

    using SessionCache = ginnunga::SparseCache<
        IPv6_Port_Pair,
        SessionState,
        ginnunga::storage::SimdFlatMap<IPv6_Port_Pair, SessionState, IPv6Hash>,
        ginnunga::eviction::ARCPolicy<IPv6_Port_Pair>,
        ginnunga::concurrency::EpochReclamation
    >;

    SessionCache tracker(5000);

    // Generate sample IPv6 endpoints
    std::vector<IPv6_Port_Pair> active_sessions;
    for (int i = 1; i <= 8; ++i) {
        IPv6_Port_Pair ep{};
        ep.ip[0] = 0x20; ep.ip[1] = 0x01; // 2001:db8::
        ep.ip[2] = 0x0d; ep.ip[3] = 0xb8;
        ep.ip[15] = static_cast<uint8_t>(i);
        ep.port = static_cast<uint16_t>(8000 + i);

        SessionState state{};
        state.state = 1; // ESTABLISHED
        state.packets_forwarded = 100 * i;
        state.bytes_transferred = 64000 * i;

        tracker.put(ep, state);
        active_sessions.push_back(ep);
    }

    std::cout << "[*] Seeded " << tracker.size() << " active IPv6 sessions into SIMD flat map.\n";

    // Simulate concurrent packet routing threads with wait-free reads via EBR
    std::cout << "[*] Spawning 2 worker packet routing threads (EBR wait-free reads)...\n";
    std::vector<std::thread> workers;

    for (int t = 0; t < 2; ++t) {
        workers.emplace_back([&tracker, &active_sessions, t]() {
            for (int r = 0; r < 5; ++r) {
                const auto& ep = active_sessions[(r + t) % active_sessions.size()];
                if (auto s = tracker.get(ep); s.has_value()) {
                    // Reader executed inside EBR epoch
                }
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    std::cout << "  -> Packet routing read rounds completed successfully.\n";

    // Inspect one session
    if (auto s = tracker.get(active_sessions[0]); s.has_value()) {
        std::cout << "\n[*] Session 2001:db8::1:8001:\n";
        std::cout << "  Status:              " << (s->state == 1 ? "ESTABLISHED" : "OTHER") << "\n";
        std::cout << "  Packets Forwarded:   " << s->packets_forwarded << "\n";
        std::cout << "  Bytes Transferred:   " << s->bytes_transferred << " bytes\n";
    }

    std::cout << "\n[*] Adaptive Replacement Cache (ARC) state:\n";
    std::cout << "  ARC target p (recency bias): " << tracker.eviction().target_p() << "\n";

    std::cout << "\n[*] Telemetry JSON Dump:\n";
    std::cout << tracker.dump_telemetry_json("IPv6_TCP_Session_Tracker") << "\n";

    return 0;
}
