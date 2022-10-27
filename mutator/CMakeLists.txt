cmake_minimum_required(VERSION 3.4)
project(afl-ir-custom-mutator)

find_package(LLVM REQUIRED PATHS $ENV{FUZZING_HOME}/$ENV{LLVM}/build-debug/lib/cmake/llvm NO_DEFAULT_PATH)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(CMAKE_CXX_FLAGS "-std=c++17 -fno-rtti -Wall -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

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
    $ENV{FUZZING_HOME}/$ENV{AFL}/include/
    ${LLVM_INCLUDE_DIRS}
    ./include/
)
add_definitions(${LLVM_DEFINITIONS})

add_library(AFLCustomIRMutator SHARED
    src/afl-mutator.c
    src/mutator.cpp
)
target_link_libraries(AFLCustomIRMutator
    ${LLVM_LIBS}
)

add_library(AFLFuzzMutate SHARED
    src/afl-mutator.c
    src/fuzzmutate.cpp
)
target_link_libraries(AFLFuzzMutate
    ${LLVM_LIBS}
)


add_executable(MutatorDriver
    src/main.cpp
)
target_link_libraries(MutatorDriver
    AFLCustomIRMutator
    ${LLVM_LIBS}
)
