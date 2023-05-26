# ---------------------------------------------------------------------------
# BtrBlocks library source code
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Includes
# ---------------------------------------------------------------------------
set(BTR_CC_LINTER_IGNORE)

# ---------------------------------------------------------------------------
# Library Options
# ---------------------------------------------------------------------------

if (${NO_SIMD})
    message("Disabling SIMD for btrblocks")
    add_compile_definitions(BTR_FLAG_NO_SIMD=1)
endif()

if (${WITH_LOGGING})
    message("Enabling logging for btrblocks")
    add_compile_definitions(BTR_FLAG_LOGGING=1)
endif()

# ---------------------------------------------------------------------------
# Files
# ---------------------------------------------------------------------------

file(GLOB_RECURSE BTR_CC btrblocks/**/**.cpp btrblocks/**.cpp btrblocks/**.hpp btrblocks/**.c btrblocks/**.h)
if (NOT UNIX)
    message(SEND_ERROR "unsupported platform")
endif ()

# Gather lintable files
set(BTR_CC_LINTING "")
foreach (SRC_FILE ${BTR_CC})
    list(FIND BTR_CC_LINTER_IGNORE "${SRC_FILE}" SRC_FILE_IDX)
    if (${SRC_FILE_IDX} EQUAL -1)
        list(APPEND BTR_CC_LINTING "${SRC_FILE}")
    endif ()
endforeach ()

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------

if (${BUILD_SHARED_LIBRARY})
    add_library(btrblocks SHARED ${BTR_CC})
else ()
    add_library(btrblocks STATIC ${BTR_CC})
endif ()

if (CMAKE_BUILD_TYPE MATCHES Debug)
    # CMAKE_<LANG>_STANDARD_LIBRARIES
    # target_compile_options(btrblocks PUBLIC -fsanitize=address)
    target_compile_options(btrblocks PUBLIC -g)
endif ()

target_link_libraries(btrblocks Threads::Threads fsst fastpfor croaring) #asan
# TODO(open-sourcing) dependencies i'd like to remove from the core library
target_link_libraries(btrblocks yaml tbb gflags)

if (${WITH_LOGGING})
    target_include_directories(btrblocks PUBLIC ${SPDLOG_INCLUDE_DIR})
    add_dependencies(btrblocks spdlog_src)
endif()

set(BTR_PUBLIC_INCLUDE_DIR ${PUBLIC_INCLUDE_DIRECTORY})
set(BTR_PRIVATE_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR} ${PUBLIC_INCLUDE_DIRECTORY}/btrblocks)
set(BTR_INCLUDE_DIR ${BTR_PUBLIC_INCLUDE_DIR} ${BTR_PRIVATE_INCLUDE_DIR})

target_include_directories(btrblocks
    PUBLIC ${BTR_PUBLIC_INCLUDE_DIR}
    PRIVATE ${BTR_PRIVATE_INCLUDE_DIR})

# set_property(TARGET btrblocks APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${BTR_INCLUDE_DIR})

# ---------------------------------------------------------------------------
# Linting
# ---------------------------------------------------------------------------

add_clang_tidy_target(lint_src "${BTR_CC_LINTING}")
list(APPEND lint_targets lint_src)
