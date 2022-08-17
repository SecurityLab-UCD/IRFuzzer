#include "mutator.h"

#include "llvm/FuzzMutate/FuzzerCLI.h"
#include "llvm/FuzzMutate/IRMutator.h"
#include "llvm/FuzzMutate/Operations.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vector>

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "I need a file to mutate on");
    exit(1);
  }
  std::ifstream infile(argv[1], std::ios::binary | std::ios::ate);
  std::streamsize size = infile.tellg();
  infile.seekg(0, std::ios::beg);

  std::vector<char> buffer(size + 1024);
  if (infile.read(buffer.data(), size)) {
    srand(time(NULL));
    createISelMutator();
    size_t newSize = LLVMFuzzerCustomMutator((uint8_t *)buffer.data(), size,
                                             size + 1024, rand());
    std::ofstream outbc =
        std::ofstream("out.bc", std::ios::out | std::ios::binary);
    outbc.write(buffer.data(), size);
    outbc.close();

    llvm::LLVMContext Context;
    std::unique_ptr<llvm::Module> M =
        llvm::parseModule((uint8_t *)buffer.data(), newSize, Context);

    std::error_code EC;
    llvm::raw_fd_ostream outll("out.ll", EC);
    M->print(outll, nullptr);
  } else {
    fprintf(stderr, "I can't read the file.");
  }
  infile.close();
  return 0;
}