# ---------------------------------------------------------------------------
# BtrBlocks
# ---------------------------------------------------------------------------

include(ExternalProject)
find_package(Git REQUIRED)

# Import google double converisons https://github.com/google/double-conversion
ExternalProject_Add(gdouble_src
                    PREFIX vendor/gdouble
                    GIT_REPOSITORY https://github.com/google/double-conversion.git
                    GIT_TAG cb5cf996d123014a2420c853c4db60e4500973b1
                    URL_MD5 ${GDOUGDOUBLE_URL_MD5}
                    CONFIGURE_COMMAND "cmake" "." "-DBUILD_TESTING=ON"
                    BUILD_COMMAND  make
                    BUILD_IN_SOURCE 1
                    INSTALL_COMMAND ""
                    UPDATE_COMMAND ""
                    LOG_DOWNLOAD 1
                    LOG_BUILD 1
                    STEP_TARGETS ${GDOUBLE_PREFIX}_info ${GDOUBLE_EXAMPLES_STEP}
                    )

ExternalProject_Get_property(gdouble_src source_dir)
set(GDOUBLE_ROOT_DIR ${source_dir})

# Prepare gdouble
set(GDOUBLE_INCLUDE_DIR ${GDOUBLE_ROOT_DIR})
set(GDOUBLE_LIBRARY_PATH ${GDOUBLE_ROOT_DIR}/libdouble-conversion.a)
file(MAKE_DIRECTORY ${GDOUBLE_INCLUDE_DIR})

add_library(gdouble STATIC IMPORTED)
set_property(TARGET gdouble PROPERTY IMPORTED_LOCATION ${GDOUBLE_LIBRARY_PATH})
set_property(TARGET gdouble APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${GDOUBLE_INCLUDE_DIR})

# Dependencies
add_dependencies(gdouble gdouble_src)
