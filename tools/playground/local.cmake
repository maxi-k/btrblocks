# ---------------------------------------------------------------------------
# BtrBlocks Playground
# ---------------------------------------------------------------------------

set(BTR_PLAYGROUND_DIR ${CMAKE_CURRENT_LIST_DIR})

add_executable(playground ${BTR_PLAYGROUND_DIR}/playground.cpp)
target_include_directories(playground PRIVATE ${BTR_INCLUDE_DIR})
target_link_libraries(playground btrblocks fsst gflags Threads::Threads asan fastpfor)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    # CMAKE_<LANG>_STANDARD_LIBRARIES
    target_compile_options(playground PRIVATE -fsanitize=address)
endif()

# ---------------------------------------------------------------------------
# Playing around with individual compression schemes & BtrBlocks components
# ---------------------------------------------------------------------------

# Run Length Encoding
add_executable(rle ${BTR_PLAYGROUND_DIR}/rle.cpp)
target_include_directories(rle PRIVATE ${BTR_INCLUDE_DIR})
target_link_libraries(rle btrblocks fastpfor gflags Threads::Threads)

# FSST
add_executable(fsst_0 ${BTR_PLAYGROUND_DIR}/fsst_0.cpp)
target_include_directories(fsst_0 PRIVATE ${BTR_INCLUDE_DIR})
target_compile_options(fsst_0 PRIVATE -fsanitize=address)
target_link_libraries(fsst_0 btrblocks fsst pthread asan)

# Frame of Reference
add_executable(for_tests ${BTR_PLAYGROUND_DIR}/for_tests.cpp)
target_include_directories(for_tests PRIVATE ${BTR_INCLUDE_DIR})
target_link_libraries(for_tests btrblocks gflags)

# Google Double
add_executable(p_double ${BTR_PLAYGROUND_DIR}/double.cpp)
target_include_directories(p_double PRIVATE ${BTR_INCLUDE_DIR} ${GDOUBLE_INCLUDE_DIR})
target_link_libraries(p_double gdouble btrblocks)

# Google Double (Benchmark)
add_executable(double_benchmarking ${BTR_PLAYGROUND_DIR}/double_benchmarking.cpp)
target_include_directories(double_benchmarking PRIVATE ${BTR_INCLUDE_DIR} ${GDOUBLE_INCLUDE_DIR})
target_link_libraries(double_benchmarking gdouble btrblocks)

# Pseudodecimal
add_executable(pseudodecimal_benchmark ${BTR_PLAYGROUND_DIR}/pseudodecimal_benchmark.cpp)
target_include_directories(pseudodecimal_benchmark PRIVATE ${BTR_INCLUDE_DIR})
target_link_libraries(pseudodecimal_benchmark btrblocks gflags fsst pthread spdlog)
configure_file(${BTR_PLAYGROUND_DIR}/pbi-double-columns.txt pbi-double-columns.txt COPYONLY)

# Sampling Algorithm
add_executable(sampling_algorithms ${BTR_PLAYGROUND_DIR}/sampling_algorithms.cpp)
target_include_directories(sampling_algorithms PRIVATE ${BTR_INCLUDE_DIR})
target_link_libraries(sampling_algorithms btrblocks gflags spdlog)
configure_file(${BTR_PLAYGROUND_DIR}/s3-columns.txt s3-columns.txt COPYONLY)


# Configuration Mechanism
add_executable(threadlocal_config ${BTR_PLAYGROUND_DIR}/config.cpp)
target_link_libraries(threadlocal_config btrblocks tbb)

# ---------------------------------------------------------------------------
# Playing around with the AWS S3 SDK & API
# ---------------------------------------------------------------------------

add_executable(test-s3 ${BTR_PLAYGROUND_DIR}/test-s3.cpp)
target_link_libraries(test-s3 libaws-cpp-sdk-core libaws-cpp-sdk-s3)

add_executable(test-s3-crt ${BTR_PLAYGROUND_DIR}/test-s3-crt.cpp)
target_link_libraries(test-s3-crt libaws-cpp-sdk-core libaws-cpp-sdk-s3-crt libaws-cpp-sdk-s3)

add_executable(test-s3-custom-stream ${BTR_PLAYGROUND_DIR}/test-s3-custom-stream.cpp)
target_link_libraries(test-s3-custom-stream libaws-cpp-sdk-core libaws-cpp-sdk-s3)

add_executable(test-s3-transfer ${BTR_PLAYGROUND_DIR}/test-s3-transfer.cpp)
target_link_libraries(test-s3-transfer libaws-cpp-sdk-core libaws-cpp-sdk-s3 libaws-cpp-sdk-transfer)

add_executable(generate_s3_data ${BTR_PLAYGROUND_DIR}/generate_s3_data.cpp)
target_link_libraries(generate_s3_data libaws-cpp-sdk-core libaws-cpp-sdk-s3-crt tbb)
