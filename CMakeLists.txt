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

cmake_minimum_required(VERSION 3.22)
project(Ginnunga VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Check compiler support for C++26, fallback to C++20 if needed
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU|AppleClang")
    message(WARNING "Ginnunga is optimized for Clang, GCC, and AppleClang.")
endif()

# Compiler optimization and warning flags
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
)

if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
    add_compile_options(-O3 -fomit-frame-pointer)
endif()

# Architecture specific SIMD detection
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    message(STATUS "Configuring for ARM64 / Apple Silicon with NEON SIMD optimizations")
    add_compile_definitions(GINNUNGA_ARM_NEON=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    message(STATUS "Configuring for x86_64 with AVX2 SIMD optimizations")
    add_compile_options(-mavx2)
    add_compile_definitions(GINNUNGA_X86_AVX2=1)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Main interface library
add_library(ginnunga INTERFACE)
target_include_directories(ginnunga INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Optional compiled source runtime
add_library(ginnunga_core STATIC src/ginnunga.cpp)
target_link_libraries(ginnunga_core PUBLIC ginnunga)

option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)
if(ENABLE_COVERAGE)
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping -O0 -g)
    add_link_options(-fprofile-instr-generate)
endif()

# Enable testing
enable_testing()

add_executable(ginnunga_tests
    tests/test_main.cpp
    tests/test_concepts.cpp
    tests/test_storage.cpp
    tests/test_eviction.cpp
    tests/test_concurrency.cpp
    tests/test_memory.cpp
    tests/test_telemetry.cpp
    tests/test_cache.cpp
)
target_link_libraries(ginnunga_tests PRIVATE ginnunga ginnunga_core)
add_test(NAME GinnungaUnitTests COMMAND ginnunga_tests)

# Microbenchmarks
add_executable(ginnunga_bench bench/bench_cache.cpp)
target_link_libraries(ginnunga_bench PRIVATE ginnunga ginnunga_core)

# Application Examples
add_executable(example_fix_order examples/fix_order_cache.cpp)
target_link_libraries(example_fix_order PRIVATE ginnunga ginnunga_core)

add_executable(example_ecs_component examples/ecs_component_cache.cpp)
target_link_libraries(example_ecs_component PRIVATE ginnunga ginnunga_core)

add_executable(example_tcp_session examples/tcp_session_tracker.cpp)
target_link_libraries(example_tcp_session PRIVATE ginnunga ginnunga_core)
