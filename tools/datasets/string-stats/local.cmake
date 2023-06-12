# ---------------------------------------------------------------------------
# BtrBlocks String Stats
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Files
# ---------------------------------------------------------------------------

add_executable(string_stats ${CMAKE_CURRENT_LIST_DIR}/StringStats.cpp)
target_include_directories(string_stats PRIVATE ${BTR_INCLUDE_DIR})
target_link_libraries(string_stats btrblocks Threads::Threads gflags)


add_executable(string_fsst ${CMAKE_CURRENT_LIST_DIR}/StringFSST.cpp)
target_include_directories(string_fsst PRIVATE ${BTR_INCLUDE_DIR})
target_link_libraries(string_fsst btrblocks Threads::Threads gflags fsst)

