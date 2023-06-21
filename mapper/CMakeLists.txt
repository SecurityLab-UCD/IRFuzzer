cmake_minimum_required(VERSION 3.4)
project(map2patterns)


if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
  message(STATUS "Build type not specified: Use Release by default")
endif(NOT CMAKE_BUILD_TYPE)

IF(CMAKE_BUILD_TYPE MATCHES Release)
    set(LLVM_DIR $ENV{FUZZING_HOME}/$ENV{LLVM}/build-release/lib/cmake/llvm)
ENDIF(CMAKE_BUILD_TYPE MATCHES Release)

IF(CMAKE_BUILD_TYPE MATCHES Debug)
    set(LLVM_DIR $ENV{FUZZING_HOME}/$ENV{LLVM}/build-debug/lib/cmake/llvm)
ENDIF(CMAKE_BUILD_TYPE MATCHES Debug)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(CMAKE_CXX_FLAGS "-std=c++17 -fno-rtti -Wall -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

llvm_map_components_to_libnames(LLVM_LIBS
    Core
    Support
)

include_directories(
    ${LLVM_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
)
add_definitions(${LLVM_DEFINITIONS})

# TODO: use apt to install simdjson
file(DOWNLOAD https://github.com/simdjson/simdjson/releases/download/v3.1.7/simdjson.h ${CMAKE_CURRENT_SOURCE_DIR}/include/simdjson.h)
file(DOWNLOAD https://github.com/simdjson/simdjson/releases/download/v3.1.7/simdjson.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/simdjson.cpp)

add_library(shadow SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/src/matchertree.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shadowmap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/predicate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/simdjson.cpp
)
add_executable(mapper
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mapper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/commandline.cpp
)
target_link_libraries(shadow
    ${LLVM_LIBS}
)
target_link_libraries(mapper
    shadow
)