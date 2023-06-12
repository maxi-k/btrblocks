# ---------------------------------------------------------------------------
# BtrBlocks Double Stats
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Files
# ---------------------------------------------------------------------------

add_executable(double_stats ${CMAKE_CURRENT_LIST_DIR}/DoubleStatsExec.cpp)
target_include_directories(double_stats PRIVATE ${BTR_INCLUDE_DIR})
target_link_libraries(double_stats btrblocks Threads::Threads gflags)
