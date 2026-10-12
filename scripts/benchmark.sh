#!/usr/bin/env bash
set -euo pipefail

# Project Ginnunga - Microbenchmark Runner Script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

if [[ ! -f "${BUILD_DIR}/ginnunga_bench" ]]; then
    echo "[*] Benchmark binary not found. Running build script first..."
    "${SCRIPT_DIR}/build.sh"
fi

"${BUILD_DIR}/ginnunga_bench"
