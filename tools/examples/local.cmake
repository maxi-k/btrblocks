# ---------------------------------------------------------------------------
# BtrBlocks Examples
# ---------------------------------------------------------------------------

set(BTR_EXAMPLES_DIR ${CMAKE_CURRENT_LIST_DIR})

add_executable(example_compression ${BTR_EXAMPLES_DIR}/compression.cpp)
target_link_libraries(example_compression btrblocks)
