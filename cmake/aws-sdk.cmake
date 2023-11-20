# ---------------------------------------------------------------------------
# btrblocks
# ---------------------------------------------------------------------------

include(ExternalProject)
find_package(Git REQUIRED)

# libawscpp
ExternalProject_Add(
        libawscpp-download
        PREFIX "vendor/libawscpp-download"
        GIT_REPOSITORY "https://github.com/aws/aws-sdk-cpp.git"
        GIT_TAG "1.11.205"
        TIMEOUT 10
        LIST_SEPARATOR "|"
        CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/vendor/libawscpp-download
        -DCMAKE_INSTALL_LIBDIR=lib
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
        -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
        -DENABLE_TESTING=OFF # Test are broken with gcc-11, gcc-10 leads to errors about missing atomic operations. Gave up and disabled tests.
        -DBUILD_ONLY=s3|s3-crt|transfer
        UPDATE_COMMAND ""
        TEST_COMMAND ""
)

# Prepare libaws-cpp-sdk-core
ExternalProject_Get_Property(libawscpp-download install_dir)
set(AWS_INCLUDE_DIR ${install_dir}/include)
if (${ARM_BUILD})
    set(AWS_LIBRARY_PATH ${install_dir}/lib/libaws-cpp-sdk-core.dylib)
else()
    set(AWS_LIBRARY_PATH ${install_dir}/lib/libaws-cpp-sdk-core.so)
endif()
file(MAKE_DIRECTORY ${AWS_INCLUDE_DIR})
add_library(libaws-cpp-sdk-core SHARED IMPORTED)
set_property(TARGET libaws-cpp-sdk-core PROPERTY IMPORTED_LOCATION ${AWS_LIBRARY_PATH})
set_property(TARGET libaws-cpp-sdk-core APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${AWS_INCLUDE_DIR})
add_dependencies(libaws-cpp-sdk-core libawscpp-download)

# Prepare libaws-cpp-sdk-s3
ExternalProject_Get_Property(libawscpp-download install_dir)
set(AWS_INCLUDE_DIR ${install_dir}/include)
if (${ARM_BUILD})
    set(AWS_LIBRARY_PATH ${install_dir}/lib/libaws-cpp-sdk-s3.dylib)
else()
    set(AWS_LIBRARY_PATH ${install_dir}/lib/libaws-cpp-sdk-s3.so)
endif()
file(MAKE_DIRECTORY ${AWS_INCLUDE_DIR})
add_library(libaws-cpp-sdk-s3 SHARED IMPORTED)
set_property(TARGET libaws-cpp-sdk-s3 PROPERTY IMPORTED_LOCATION ${AWS_LIBRARY_PATH})
set_property(TARGET libaws-cpp-sdk-s3 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${AWS_INCLUDE_DIR})
add_dependencies(libaws-cpp-sdk-s3 libawscpp-download)

# Prepare libaws-cpp-sdk-s3-crt
ExternalProject_Get_Property(libawscpp-download install_dir)
set(AWS_INCLUDE_DIR ${install_dir}/include)
if (${ARM_BUILD})
    set(AWS_LIBRARY_PATH ${install_dir}/lib/libaws-cpp-sdk-s3-crt.dylib)
else()
    set(AWS_LIBRARY_PATH ${install_dir}/lib/libaws-cpp-sdk-s3-crt.so)
endif()
file(MAKE_DIRECTORY ${AWS_INCLUDE_DIR})
add_library(libaws-cpp-sdk-s3-crt SHARED IMPORTED)
set_property(TARGET libaws-cpp-sdk-s3-crt PROPERTY IMPORTED_LOCATION ${AWS_LIBRARY_PATH})
set_property(TARGET libaws-cpp-sdk-s3-crt APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${AWS_INCLUDE_DIR})
add_dependencies(libaws-cpp-sdk-s3-crt libawscpp-download)

# Prepare libaws-cpp-sdk-transfer
ExternalProject_Get_Property(libawscpp-download install_dir)
set(AWS_INCLUDE_DIR ${install_dir}/include)
if (${ARM_BUILD})
    set(AWS_LIBRARY_PATH ${install_dir}/lib/libaws-cpp-sdk-transfer.dylib)
else()
    set(AWS_LIBRARY_PATH ${install_dir}/lib/libaws-cpp-sdk-transfer.so)
endif()
file(MAKE_DIRECTORY ${AWS_INCLUDE_DIR})
add_library(libaws-cpp-sdk-transfer SHARED IMPORTED)
set_property(TARGET libaws-cpp-sdk-transfer PROPERTY IMPORTED_LOCATION ${AWS_LIBRARY_PATH})
set_property(TARGET libaws-cpp-sdk-transfer APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${AWS_INCLUDE_DIR})
add_dependencies(libaws-cpp-sdk-transfer libawscpp-download)
