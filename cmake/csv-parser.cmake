# ---------------------------------------------------------------------------
# BtrBlocks
# ---------------------------------------------------------------------------

include(ExternalProject)
find_package(Git REQUIRED)

# Get rapidjson
ExternalProject_Add(
    csv_src
    PREFIX "vendor/csv-parser"
    GIT_REPOSITORY "https://github.com/AriaFallah/csv-parser"
    GIT_TAG 4965c9f
    TIMEOUT 10
    CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/vendor/csv-parser
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
)

# copy include/parser.hpp over to csv-parser/parser.hpp
ExternalProject_Get_Property(csv_src install_dir)

set(CSV_HEADER_SRC ${install_dir}/include/parser.hpp)
set(CSV_HEADER_DST ${install_dir}/include/csv-parser/parser.hpp)

file(MAKE_DIRECTORY ${install_dir}/include/csv-parser)

ExternalProject_Add_Step(csv_src copy_parser
    COMMAND ${CMAKE_COMMAND} -E rename ${CSV_HEADER_SRC} ${CSV_HEADER_DST}
    BYPRODUCTS ${CSV_HEADER_DST}
    DEPENDEES install
    WORKING_DIRECTORY ${install_dir}
)

set(CSV_INCLUDE_DIR ${install_dir}/include)

add_library(csv-parser INTERFACE)
add_dependencies(csv-parser csv_src)
target_include_directories(csv-parser SYSTEM INTERFACE ${CSV_INCLUDE_DIR})
