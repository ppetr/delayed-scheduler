# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.11)

enable_testing()

set(CMAKE_CXX_STANDARD 11)
set(CXX_STANDARD_REQUIRED ON)
# Set variables for subdirectories.
# See https://stackoverflow.com/a/3769269/1333025.
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL
    "Disable Benchmark library tests")
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "")

project(delayed-scheduler CXX)

include(FetchContent)
function(FetchLibrary)
    if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER_EQUAL 3.24)
        # Look for local packages, otherwise download from the git repo.
        list(SUBLIST ARGV 3 -1 other_names)
        FetchContent_Declare(
            ${ARGV0}
            GIT_REPOSITORY ${ARGV1}
            GIT_TAG        ${ARGV2}
            FIND_PACKAGE_ARGS REQUIRED EXCLUDE_FROM_ALL NAMES ${ARGV0} ${other_names}
            )
    else()
        FetchContent_Declare(
            ${ARGV0}
            GIT_REPOSITORY ${ARGV1}
            GIT_TAG        ${ARGV2}
            )
    endif()
endfunction()

# We need a recent version that includes `absl::AnyInvocable`.
FetchContent_Declare(
    absl
    GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
    GIT_TAG        8c0b94e793a66495e0b1f34a5eb26bd7dc672db0  # lts_2022_06_23
    )

FetchLibrary(
    googletest
    https://github.com/google/googletest.git
    e2239ee6043f73722e7aa812a459f54a28552929  # release-1.11.0
    GTest
    )

# For the benchmarks below be sure to enable optimizations, for example:
#   cmake -DBENCHMARK_ENABLE_LTO=true -DCMAKE_BUILD_TYPE=Release
# See https://github.com/google/benchmark#debug-vs-release.
FetchLibrary(
    benchmark
    https://github.com/google/benchmark.git
    db55c89f31385f8105f56ac8245a51777e94a628
    )

FetchContent_MakeAvailable(absl googletest benchmark)

# Var-sized and ref-counted structures.

add_library(delayed_scheduler INTERFACE)
target_include_directories(delayed_scheduler INTERFACE .)
# -latomic is missing on some platforms, see
# https://gitlab.kitware.com/cmake/cmake/-/issues/21174.
target_link_libraries(delayed_scheduler INTERFACE atomic absl::any_invocable absl::base absl::optional absl::synchronization absl::time)

add_executable(delayed_scheduler_test delayed_scheduler_test.cc)
target_link_libraries(delayed_scheduler_test delayed_scheduler absl::memory absl::optional absl::time absl::utility gmock gtest_main)
add_test(NAME delayed_scheduler_test COMMAND delayed_scheduler_test)
