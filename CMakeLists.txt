cmake_minimum_required(VERSION 3.0)
project(kvraft)

enable_testing()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

if(NOT CMAKE_STUB_FORMATTER)
    set(CMAKE_STUB_FORMATTER echo)
endif()

if(NOT TARGET Protobuf)
    find_package(Protobuf REQUIRED)
endif()
if(Protobuf_FOUND)
    include_directories(${Protobuf_INCLUDE_DIRS})
else()
    message(FATAL_ERROR "Protobuf not found")
endif()

if(NOT TARGET gRPC)
    find_package(gRPC REQUIRED)
endif()
find_package(gRPC REQUIRED)
if(gRPC_FOUND)
    include_directories(${gRPC_INCLUDE_DIRS})
else()
    message(FATAL_ERROR "gRPC not found")
endif()

set(CXX_FLAGS
        -fno-omit-frame-pointer # linux perf
        -Wall
        -Wextra
        -Werror
        -Wconversion
        -Wno-unused-parameter
        -Wold-style-cast
        -Woverloaded-virtual
        -Wpointer-arith
        -Wshadow
        -Wwrite-strings
        -std=c++17
        -march=native
        -rdynamic)
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

find_path(LEVELDB_INCLUDE_PATH NAMES leveldb/db.h)
find_library(LEVELDB_LIB NAMES leveldb)
if ((NOT LEVELDB_INCLUDE_PATH) OR (NOT LEVELDB_LIB))
    message(FATAL_ERROR "Fail to find leveldb")
endif()

include_directories(
        raft
        ${LEVELDB_INCLUDE_PATH}
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_BINARY_DIR})
add_subdirectory(raft)
add_subdirectory(example)