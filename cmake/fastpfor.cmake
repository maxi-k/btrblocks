# ---------------------------------------------------------------------------
# btrblocks
# ---------------------------------------------------------------------------

include(ExternalProject)
find_package(Git REQUIRED)

ExternalProject_Add(
    fastpfor_src
    PREFIX "vendor/lemire/fastpfor"
    GIT_REPOSITORY "https://github.com/lemire/FastPFor.git"
    GIT_TAG bc375056b803af9b3ca5a6c98e6b38aeca80d825
    TIMEOUT 10
    UPDATE_COMMAND "" # to prevent rebuilding everytime
    INSTALL_COMMAND ""
    CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/vendor/fastpfor_cpp
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
)

ExternalProject_Get_Property(fastpfor_src source_dir)
ExternalProject_Get_Property(fastpfor_src binary_dir)

set(FASTPFOR_INCLUDE_DIR ${source_dir}/)
set(FASTPFOR_LIBRARY_PATH ${binary_dir}/libFastPFOR.a)

file(MAKE_DIRECTORY ${FASTPFOR_INCLUDE_DIR})

add_library(fastpfor STATIC IMPORTED)
add_dependencies(fastpfor fastpfor_src)
target_include_directories(fastpfor SYSTEM INTERFACE ${FASTPFOR_INCLUDE_DIR})

set_property(TARGET fastpfor PROPERTY IMPORTED_LOCATION ${FASTPFOR_LIBRARY_PATH})