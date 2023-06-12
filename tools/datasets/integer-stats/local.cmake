# ---------------------------------------------------------------------------
# BtrBlocks Integer Stats
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Files
# ---------------------------------------------------------------------------

add_executable(integer_stats ${CMAKE_CURRENT_LIST_DIR}/IntegerStats.cpp)
target_include_directories(integer_stats PRIVATE ${BTR_INCLUDE_DIR})
target_link_libraries(integer_stats btrblocks Threads::Threads gflags)
