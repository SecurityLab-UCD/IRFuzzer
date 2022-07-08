# adapted a bit from http://llvm.org/docs/CMake.html#embedding-llvm-in-your-project

cmake_minimum_required(VERSION 3.4)
project(afl-isel-driver)

# Currently, we have to move this project to `llvm-aie/llvm/tools/` so it can compile.
# The driver can only test targets that belong to the compiler that compiled it.
# Since AIE is not self-hosting, we have to compile it _with_ AIE.
# Once it is self hosting, you can compile this project using AIE-clang.

# find_package(LLVM REQUIRED CONFIG)
# message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
# message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
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
    afl-compiler-rt.o
)
target_compile_options(isel-fuzzing PRIVATE -fno-rtti)

target_link_libraries(isel-fuzzing
    ${LLVM_LIBS}
)