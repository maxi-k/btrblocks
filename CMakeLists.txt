# -------------------------------------------------------------------------------
# BtrBlocks
# -------------------------------------------------------------------------------
project(BtrBlocks)
cmake_minimum_required(VERSION 3.7)

# -------------------------------------------------------------------------------
# Define compile-time flags
# -------------------------------------------------------------------------------

option(NO_SIMD "NO_SIMD" OFF)
option(WITH_LOGGING "WITH_LOGGING" OFF)
option(BUILD_SHARED_LIBRARY "BUILD_SHARED_LIBRARY" OFF)

# -------------------------------------------------------------------------------
# C++ build options
# -------------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -g")
add_compile_options(-march=native -Wall -Wextra)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include("cmake/environment.cmake") # Set environment-specific compiler flags
include("cmake/clang-tidy.cmake") # Integrate clang-tidy

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

find_package(Threads REQUIRED)
set(THREADS_PREFER_PTHREAD_FLAG ON)

set(CMAKE_VENDOR_DIR "${CMAKE_SOURCE_DIR}/vendor/")

# Compression library dependencies
include("${CMAKE_VENDOR_DIR}/fastpfor.cmake")
include("${CMAKE_VENDOR_DIR}/tbb.cmake")
include("${CMAKE_VENDOR_DIR}/fsst.cmake")
include("${CMAKE_VENDOR_DIR}/croaring.cmake")

# Benchmark dependencies
include("${CMAKE_VENDOR_DIR}/aws-sdk.cmake")
include("${CMAKE_VENDOR_DIR}/benchmark.cmake")
include("${CMAKE_VENDOR_DIR}/googletest.cmake")
include("${CMAKE_VENDOR_DIR}/gflags.cmake")
include("${CMAKE_VENDOR_DIR}/yaml-cpp.cmake")
include("${CMAKE_VENDOR_DIR}/spdlog.cmake")
include("${CMAKE_VENDOR_DIR}/gdouble.cmake")
include("${CMAKE_VENDOR_DIR}/turbo.cmake")
include("${CMAKE_VENDOR_DIR}/lz4.cmake")

# ---------------------------------------------------------------------------
# Components
# ---------------------------------------------------------------------------

include("${CMAKE_SOURCE_DIR}/include/local.cmake")    # public headers
include("${CMAKE_SOURCE_DIR}/btrblocks/local.cmake")  # btrblocks library
include("${CMAKE_SOURCE_DIR}/tools/local.cmake")      # btrblocks tooling
include("${CMAKE_SOURCE_DIR}/benchmarks/local.cmake") # btrblocks benchmarks

# ---------------------------------------------------------------------------
# Linting
# ---------------------------------------------------------------------------

if (${lint_targets})
  add_custom_target(lint)
  add_dependencies(lint ${lint_targets})
endif()

# ---------------------------------------------------------------------------
# Print Configuration
# ---------------------------------------------------------------------------

message(STATUS "[compiler] settings")
message(STATUS "     CMAKE_CXX_FLAGS            = ${CMAKE_CXX_FLAGS}")
message(STATUS "     CMAKE_CXX_FLAGS_RELEASE    = ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "[btrblocks] settings")
message(STATUS "    NO_SIMD                     = ${NO_SIMD}")
message(STATUS "    WITH_LOGGING                = ${WITH_LOGGING}")
message(STATUS "    BUILD_SHARED_LIBRARY        = ${BUILD_SHARED_LIBRARY}")
message(STATUS "[test] settings")
message(STATUS "    GTEST_INCLUDE_DIR           = ${GTEST_INCLUDE_DIR}")
message(STATUS "    GTEST_LIBRARY_PATH          = ${GTEST_LIBRARY_PATH}")
