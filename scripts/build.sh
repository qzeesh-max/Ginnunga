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
