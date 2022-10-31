#include "mutator.h"

#include "llvm/FuzzMutate/FuzzerCLI.h"
#include "llvm/FuzzMutate/IRMutator.h"
#include "llvm/FuzzMutate/Operations.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#define MAX_SIZE 1048576

// https://stackoverflow.com/questions/322938/recommended-way-to-initialize-srand
unsigned long mix(unsigned long a, unsigned long b, unsigned long c) {
  a = a - b;
  a = a - c;
  a = a ^ (c >> 13);
  b = b - c;
  b = b - a;
  b = b ^ (a << 8);
  c = c - a;
  c = c - b;
  c = c ^ (b >> 13);
  a = a - b;
  a = a - c;
  a = a ^ (c >> 12);
  b = b - c;
  b = b - a;
  b = b ^ (a << 16);
  c = c - a;
  c = c - b;
  c = c ^ (b >> 5);
  a = a - b;
  a = a - c;
  a = a ^ (c >> 3);
  b = b - c;
  b = b - a;
  b = b ^ (a << 10);
  c = c - a;
  c = c - b;
  c = c ^ (b >> 15);
  return c;
}
int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "I need a file to mutate on");
    exit(1);
  }
  std::ifstream infile(argv[1], std::ios::binary | std::ios::ate);
  std::streamsize size = infile.tellg();
  infile.seekg(0, std::ios::beg);

  std::vector<char> buffer(MAX_SIZE);
  if (infile.read(buffer.data(), size)) {
    srand(mix(clock(), time(NULL), getpid()));
    createISelMutator();
    unsigned int Seed = rand();
    if (argc > 2) {
      Seed = atoi(argv[2]);
    }
    bool validateMode = false;
    if (argc > 3 && argv[3][1] == 'v') {
      validateMode = true;
    }
    size_t newSize =
        LLVMFuzzerCustomMutator((uint8_t *)buffer.data(), size, MAX_SIZE, Seed);
    if (!validateMode) {
      std::ofstream outbc =
          std::ofstream("out.bc", std::ios::out | std::ios::binary);
      outbc.write(buffer.data(), newSize);
      outbc.close();
    }
    llvm::LLVMContext Context;
    std::unique_ptr<llvm::Module> M =
        llvm::parseModule((uint8_t *)buffer.data(), newSize, Context);
#ifdef DEBUG
    if (!validateMode)
      M->dump();
#endif
    /*
    std::error_code EC;
    llvm::raw_fd_ostream outll("out.ll", EC);
    M->print(outll, nullptr);
    */
    // llvm::errs() << "Verifing Module...";
    if (verifyModule(*M, &llvm::errs(), nullptr)) {
      llvm::errs() << "Verifier failed. Seed: " << Seed << "\n";
      // llvm::errs() << *M << "\n";
    } else {
      // llvm::errs() << "Good.\n";
    }
  } else {
    fprintf(stderr, "I can't read the file.");
  }
  infile.close();
  return 0;
}