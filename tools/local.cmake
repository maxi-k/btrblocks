# ---------------------------------------------------------------------------
# BtrBlocks Tooling
# ---------------------------------------------------------------------------

set(BTR_TOOLS_DIR ${CMAKE_CURRENT_LIST_DIR})

add_executable(btrtocsv ${BTR_TOOLS_DIR}/btrtocsv.cpp)
target_include_directories(btrtocsv PRIVATE ${BTR_INCLUDE_DIR})
target_link_libraries(btrtocsv btrblocks gflags)
