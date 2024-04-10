set(BTR_BENCHMARK_DIR ${CMAKE_CURRENT_LIST_DIR})

# Pseudodecimal
add_executable(column_benchmark ${BTR_BENCHMARK_DIR}/column_benchmark.cpp)
target_include_directories(column_benchmark PRIVATE ${BTR_INCLUDE_DIR})
target_link_libraries(column_benchmark btrblocks gflags tbb pthread spdlog)
configure_file(${BTR_BENCHMARK_DIR}/pbi-double-columns.txt pbi-double-columns.txt COPYONLY)