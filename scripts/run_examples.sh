#!/usr/bin/env bash

# SPDX-License-Identifier: AGPL-3.0-or-later
# Project Ginnunga: High-Performance C++ Sparse Cache Algorithm Framework
# Copyright (C) 2026 Zeeshan Qazi
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

set -euo pipefail

# Project Ginnunga - Application Examples Runner Script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

# Ensure binaries exist
if [[ ! -f "${BUILD_DIR}/example_fix_order" ]] || \
   [[ ! -f "${BUILD_DIR}/example_ecs_component" ]] || \
   [[ ! -f "${BUILD_DIR}/example_tcp_session" ]]; then
    echo "[*] Example binaries missing. Running build script first..."
    "${SCRIPT_DIR}/build.sh"
fi

TARGET="${1:-all}"

run_fix_order() {
    echo ""
    echo "========================================================"
    echo "  Example 1: Exchange FIX Order Routing Cache"
    echo "  (PagedSparseArray + TTL Policy)"
    echo "========================================================"
    "${BUILD_DIR}/example_fix_order"
}

run_ecs() {
    echo ""
    echo "========================================================"
    echo "  Example 2: Game Engine ECS Component Cache"
    echo "  (SlotArrayMap / Sparse Set + LRU Policy)"
    echo "========================================================"
    "${BUILD_DIR}/example_ecs_component"
}

run_tcp() {
    echo ""
    echo "========================================================"
    echo "  Example 3: Userspace IPv6 TCP Session Tracker"
    echo "  (SimdFlatMap + ARC Policy + EpochReclamation)"
    echo "========================================================"
    "${BUILD_DIR}/example_tcp_session"
}

case "${TARGET}" in
    fix_order|order|1)
        run_fix_order
        ;;
    ecs|component|2)
        run_ecs
        ;;
    tcp|session|3)
        run_tcp
        ;;
    all)
        run_fix_order
        run_ecs
        run_tcp
        ;;
    *)
        echo "Unknown example: ${TARGET}"
        echo "Usage: $0 [all|fix_order|ecs|tcp]"
        exit 1
        ;;
esac
