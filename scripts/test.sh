#!/usr/bin/env bash

# SPDX-License-Identifier: AGPL-3.0-or-later
# Project Ginnunga: High-Performance C++ Sparse Cache Algorithm Framework
# Copyright (C) 2026 Zeeshan Qazi <zeeshan@zeeshan.im>
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
