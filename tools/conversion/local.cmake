# ---------------------------------------------------------------------------
# BtrBlocks Conversion
# ---------------------------------------------------------------------------

set(BTR_CONVERSION_DIR ${CMAKE_CURRENT_LIST_DIR})

# ---------------------------------------------------------------------------
# Executables
# ---------------------------------------------------------------------------

add_executable(btrtocsv ${BTR_CONVERSION_DIR}/btrtocsv.cpp)
target_include_directories(btrtocsv PRIVATE ${BTR_INCLUDE_DIR})
target_link_libraries(btrtocsv btrfiles gflags tbb spdlog)

add_executable(csvtobtr ${BTR_CONVERSION_DIR}/csvtobtr.cpp)
target_include_directories(csvtobtr PRIVATE ${BTR_INCLUDE_DIR})
target_link_libraries(csvtobtr btrfiles gflags tbb spdlog)

add_executable(btrmeta ${BTR_CONVERSION_DIR}/btrmeta.cpp)
target_include_directories(btrmeta PRIVATE ${BTR_INCLUDE_DIR})
target_link_libraries(btrmeta btrblocks gflags)

add_executable(decompression-speed ${BTR_CONVERSION_DIR}/decompression-speed.cpp)
target_include_directories(decompression-speed PRIVATE ${BTR_INCLUDE_DIR})
target_link_libraries(decompression-speed btrfiles btrblocks tbb gflags)

add_executable(decompression-speed-s3 ${BTR_CONVERSION_DIR}/decompression-speed-s3.cpp)
target_include_directories(decompression-speed-s3 PRIVATE ${BTR_INCLUDE_DIR})
target_link_libraries(decompression-speed-s3 btrblocks tbb gflags libaws-cpp-sdk-core libaws-cpp-sdk-s3-crt libaws-cpp-sdk-s3)

# ---------------------------------------------------------------------------
# Scripts
# ---------------------------------------------------------------------------

configure_file(${BTR_CONVERSION_DIR}/compare_csvs.py compare_csvs.py COPYONLY)
