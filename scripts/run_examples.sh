#!/usr/bin/env bash
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
