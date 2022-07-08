llvm_map_components_to_libnames(LLVM_LIBS
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
    ${PROJECT_SOURCE_DIR}/mutator/include/
    ${PROJECT_SOURCE_DIR}/AFLplusplus/include/
)
add_definitions(${LLVM_DEFINITIONS})

add_library(AFLCustomIRMutator SHARED
    src/afl-mutator.c
    src/mutator.cpp
)

target_link_libraries(AFLCustomIRMutator
    ${LLVM_LIBS}
)
