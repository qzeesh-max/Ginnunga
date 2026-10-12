#!/usr/bin/env bash
set -euo pipefail

# Project Ginnunga - Unit Test Runner Script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

# Check if build directory and test binary exist, build if missing
if [[ ! -f "${BUILD_DIR}/ginnunga_tests" ]]; then
    echo "[*] Test binary not found. Invoking build script first..."
    "${SCRIPT_DIR}/build.sh"
fi

echo "========================================================"
echo "  Project Ginnunga - Executing Unit Test Suite"
echo "========================================================"

# Run test executable directly for verbose breakdown
"${BUILD_DIR}/ginnunga_tests"

echo ""
echo "[*] Verifying via CTest harness..."
ctest --test-dir "${BUILD_DIR}" --output-on-failure
