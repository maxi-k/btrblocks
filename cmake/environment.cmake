# ---------------------------------------------------------------------------
# Environment-specific settings
# ---------------------------------------------------------------------------
if(NOT DEFINED BTRBLOCKS_CHUNKSIZE)
    # Set BTRBLOCKS_CHUNKSIZE only if it's not already defined
    set(BTRBLOCKS_CHUNKSIZE 16)
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" AND CMAKE_BUILD_TYPE MATCHES Debug)
    add_compile_options(-fstandalone-debug)
endif()

if (CYGWIN)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++")
endif (CYGWIN)
