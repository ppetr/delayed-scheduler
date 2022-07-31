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

add_subdirectory(abseil-cpp)

add_subdirectory(googletest EXCLUDE_FROM_ALL)

# Var-sized and ref-counted structures.

add_library(delayed_scheduler INTERFACE)
target_include_directories(delayed_scheduler INTERFACE .)
# -latomic is missing on some platforms, see
# https://gitlab.kitware.com/cmake/cmake/-/issues/21174.
target_link_libraries(delayed_scheduler INTERFACE atomic absl::any_invocable absl::base absl::optional absl::synchronization absl::time)

add_executable(delayed_scheduler_test delayed_scheduler_test.cc)
target_link_libraries(delayed_scheduler_test delayed_scheduler absl::memory absl::optional absl::time gmock gtest_main)
add_test(NAME delayed_scheduler_test COMMAND delayed_scheduler_test)
