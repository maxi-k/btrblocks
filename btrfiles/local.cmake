# ---------------------------------------------------------------------------
# BtrFiles:
# Relational file compression/decompression utilities based on BtrBlocks
# ---------------------------------------------------------------------------

if (NOT UNIX)
    message(SEND_ERROR "unsupported platform")
endif ()

# ---------------------------------------------------------------------------
# Files
# ---------------------------------------------------------------------------

file(GLOB_RECURSE BTRFILES_HH btrfiles/**.hpp btrfiles/**.h)
file(GLOB_RECURSE BTRFILES_CC btrfiles/**/**.cpp btrfiles/**.cpp btrfiles/**.c)
set(BTRFILES_SRC ${BTRFILES_HH} ${BTRFILES_CC})

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------

if (${BUILD_SHARED_LIBRARY})
    add_library(btrfiles SHARED ${BTRFILES_CC})
else ()
    add_library(btrfiles STATIC ${BTRFILES_CC})
endif ()

if (CMAKE_BUILD_TYPE MATCHES Debug)
    # target_compile_options(btrfiles PUBLIC -fsanitize=address)
    target_compile_options(btrfiles PUBLIC -g)
endif ()


target_link_libraries(btrfiles btrblocks yaml csv-parser) #asan

set(BTRFILES_PUBLIC_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(BTRFILES_PRIVATE_INCLUDE_DIR ${BTR_INCLUDE_DIR})
set(BTRFILES_INCLUDE_DIR ${BTRFILES_PUBLIC_INCLUDE_DIR} ${BTRFILES_PRIVATE_INCLUDE_DIR})

target_include_directories(btrfiles
    PUBLIC ${BTRFILES_PUBLIC_INCLUDE_DIR}
    PRIVATE ${BTRFILES_PRIVATE_INCLUDE_DIR})

# set_target_properties(btrfiles PROPERTIES PUBLIC_HEADER "${BTRFILES_HH}")

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------

install(TARGETS btrfiles
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")

install(DIRECTORY ${BTRFILES_PUBLIC_INCLUDE_DIR}
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        FILES_MATCHING PATTERN "btrfiles.hpp")
