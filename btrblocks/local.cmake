# ---------------------------------------------------------------------------
# BtrBlocks library source code
# ---------------------------------------------------------------------------

set(BTR_CC_LINTER_IGNORE)
if (NOT UNIX)
    message(SEND_ERROR "unsupported platform")
endif ()

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

if (${SAMPLING_TEST_MODE})
    message("Enabling sampling test mode for btrblocks")
    add_compile_definitions(BTR_FLAG_SAMPLING_TEST_MODE=1)
endif()

if (${ENABLE_FOR_SCHEME})
    message("Enabling FOR scheme for btrblocks")
    add_compile_definitions(BTR_FLAG_ENABLE_FOR_SCHEME=1)
endif()

# ---------------------------------------------------------------------------
# Files
# ---------------------------------------------------------------------------

file(GLOB_RECURSE BTR_HH btrblocks/**.hpp  btrblocks/**.h)
file(GLOB_RECURSE BTR_CC btrblocks/**/**.cpp btrblocks/**.cpp  btrblocks/**.c)
set(BTR_SRC ${BTR_HH} ${BTR_CC})

# Gather lintable files
set(BTR_CC_LINTING "")
foreach (SRC_FILE ${BTR_SRC})
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
target_compile_options(btrblocks PUBLIC -Wno-unused-parameter)

target_link_libraries(btrblocks Threads::Threads fsst fastpfor croaring dynamic_bitset) #asan

if (${WITH_LOGGING})
    target_link_libraries(btrblocks spdlog)
endif()

# TODO including everything as public headers, as this is a research library
# later on we might want to extract a minimal public interface.
set(BTR_PUBLIC_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(BTR_PRIVATE_INCLUDE_DIR)
set(BTR_INCLUDE_DIR ${BTR_PUBLIC_INCLUDE_DIR} ${BTR_PRIVATE_INCLUDE_DIR})

target_include_directories(btrblocks
    PUBLIC ${BTR_PUBLIC_INCLUDE_DIR}
    PRIVATE ${BTR_PRIVATE_INCLUDE_DIR})

# set_target_properties(btrblocks PROPERTIES PUBLIC_HEADER "${BTR_HH}")

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------

install(TARGETS btrblocks
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")

install(DIRECTORY ${BTR_PUBLIC_INCLUDE_DIR}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")

# ---------------------------------------------------------------------------
# Linting
# ---------------------------------------------------------------------------

add_clang_tidy_target(lint_src "${BTR_CC_LINTING}")
list(APPEND lint_targets lint_src)
