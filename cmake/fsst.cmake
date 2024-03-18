# ---------------------------------------------------------------------------
# btrblocks
# ---------------------------------------------------------------------------

include(ExternalProject)
find_package(Git REQUIRED)

if (${NO_SIMD})
        message("Disabling SIMD for FSST")
        set(FSST_CXX_FLAGS -DNONOPT_FSST)
else()
        message("Enabling SIMD for FSST")
        set(FSST_CXX_FLAGS "")
endif()

ExternalProject_Add(
        fsst_src
        PREFIX "vendor/cwida/fsst"
        GIT_REPOSITORY "https://github.com/cwida/fsst.git"
        GIT_TAG c8719ef0aa3740da9685ad2738bb9c8ecc327944
        TIMEOUT 10
        UPDATE_COMMAND "" # to prevent rebuilding everytime
        INSTALL_COMMAND ""
        CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/vendor/fsst
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
)

ExternalProject_Get_Property(fsst_src source_dir)
ExternalProject_Get_Property(fsst_src binary_dir)

set(FSST_INCLUDE_DIR ${source_dir}/)
set(FSST_LIBRARY_PATH ${binary_dir}/libfsst.a)

file(MAKE_DIRECTORY ${FSST_INCLUDE_DIR})

add_library(fsst STATIC IMPORTED)
add_dependencies(fsst fsst_src)


set_property(TARGET fsst PROPERTY IMPORTED_LOCATION ${FSST_LIBRARY_PATH})
set_property(TARGET fsst APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${FSST_INCLUDE_DIR})
