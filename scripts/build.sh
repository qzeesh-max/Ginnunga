#!/usr/bin/env bash
set -euo pipefail

# Project Ginnunga - Build Script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_TYPE="${1:-Release}"
BUILD_DIR="${ROOT_DIR}/build"

echo "========================================================"
echo "  Project Ginnunga - Building (${BUILD_TYPE})"
echo "========================================================"

# Determine number of parallel jobs
if command -v sysctl >/dev/null 2>&1; then
    NPROC=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
elif command -v nproc >/dev/null 2>&1; then
    NPROC=$(nproc 2>/dev/null || echo 4)
else
    NPROC=4
fi

echo "[*] Configuring CMake in ${BUILD_DIR}..."
cmake -B "${BUILD_DIR}" -S "${ROOT_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

echo "[*] Compiling with ${NPROC} parallel jobs..."
cmake --build "${BUILD_DIR}" -j"${NPROC}"

echo "--------------------------------------------------------"
echo "  Build successful! Binaries available in build/"
echo "========================================================"
