cmake_minimum_required(VERSION 3.4)
project(llvm-isel-fuzzer)
set(AFLplusplus AFLplusplus)

find_package(LLVM REQUIRED PATHS $ENV{FUZZING_HOME}/llvm-aie/build-afl/lib/cmake/llvm NO_DEFAULT_PATH)

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
    ${LLVM_INCLUDE_DIRS}
)
add_executable(isel-fuzzing
    afl-driver.cpp
    llvm-isel-fuzzer.cpp
)
target_compile_options(isel-fuzzing PRIVATE -fno-rtti)

target_link_libraries(isel-fuzzing
    $ENV{FUZZING_HOME}/${AFLplusplus}/afl-compiler-rt.o
    ${LLVM_LIBS}
)