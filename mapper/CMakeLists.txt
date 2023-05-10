cmake_minimum_required(VERSION 3.4)
project(map2patterns)


if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
  message(STATUS "Build type not specified: Use Release by default")
endif(NOT CMAKE_BUILD_TYPE)

IF(CMAKE_BUILD_TYPE MATCHES Release)
    find_package(LLVM REQUIRED PATHS $ENV{FUZZING_HOME}/$ENV{LLVM}/build-release/lib/cmake/llvm NO_DEFAULT_PATH)
ENDIF(CMAKE_BUILD_TYPE MATCHES Release)


IF(CMAKE_BUILD_TYPE MATCHES Debug)
    find_package(LLVM REQUIRED PATHS $ENV{FUZZING_HOME}/$ENV{LLVM}/build-debug/lib/cmake/llvm NO_DEFAULT_PATH)
ENDIF(CMAKE_BUILD_TYPE MATCHES Debug)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(CMAKE_CXX_FLAGS "-std=c++17 -fno-rtti -Wall -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

# TODO: remove unnecessary components later
llvm_map_components_to_libnames(LLVM_LIBS
    AllTargetsAsmParsers
    AllTargetsCodeGens
    AllTargetsDescs
    AllTargetsInfos
    Analysis
    AsmPrinter
    BitReader
    BitWriter
    CodeGen
    Core
    FuzzMutate
    IRReader
    MC
    ScalarOpts
    SelectionDAG
    Support
    Target
)

include_directories(
    ${LLVM_INCLUDE_DIRS}
    ./include/
)
add_definitions(${LLVM_DEFINITIONS})

add_executable(mapper
    src/mapper.cpp
    src/lookup.cpp
    src/matchertree.cpp
    src/shadowmap.cpp
)

target_link_libraries(mapper
    ${LLVM_LIBS}
)