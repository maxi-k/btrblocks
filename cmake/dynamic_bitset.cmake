# ---------------------------------------------------------------------------
# boost dynamic bitset
# ---------------------------------------------------------------------------

include(FetchContent)
find_package(Git REQUIRED)

# we don't want to clone the entire boost library
# => explicitly specify the submodules we need.
# not nice, but I don't see a better solution.
set(DYNAMIC_BITSET_DEPENDENCIES
    dynamic_bitset # this is the one we need; new lines are direct requirements, afterwards transitive requirements
    assert
    config
    container_hash describe mp11 type_traits
    core
    integer
    move
    static_assert
    throw_exception)

set(BOOST_SUBMODULES ${DYNAMIC_BITSET_DEPENDENCIES})
list(TRANSFORM BOOST_SUBMODULES PREPEND "libs/")
list(APPEND BOOST_SUBMODULES ${BOOST_SUBMODULES} "tools/cmake")

message(STATUS "Fetching boost with submodules: ${BOOST_SUBMODULES}")

Set(FETCHCONTENT_QUIET FALSE)
Set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_BINARY_DIR}/vendor/boost)
FetchContent_Declare(
    boost_src
    GIT_REPOSITORY "https://github.com/boostorg/boost.git"
    GIT_TAG b6928ae5c92e21a04bbe17a558e6e066dbe632f6
    GIT_SUBMODULES ${BOOST_SUBMODULES}
    GIT_PROGRESS TRUE
    CONFIGURE_COMMAND ""  # tell CMake it's not a cmake project
    SYSTEM TRUE # this is a system library, don't lint/tidy it etc.
)

FetchContent_MakeAvailable(boost_src)

# set include directories to be system directories for all boost dependencies
# because otherwise clang-tidy will try to fix boost and fail miserably
foreach(target ${DYNAMIC_BITSET_DEPENDENCIES})
    get_target_property(target_include_dirs "boost_${target}" INTERFACE_INCLUDE_DIRECTORIES)
    set_target_properties(boost_${target} PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${target_include_dirs}")
endforeach()

add_library(dynamic_bitset INTERFACE IMPORTED)
target_link_libraries(dynamic_bitset INTERFACE Boost::dynamic_bitset)
