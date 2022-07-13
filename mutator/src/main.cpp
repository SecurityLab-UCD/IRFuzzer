#include "mutator.h"

#include <fstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "I need a file to mutate on");
    exit(1);
  }
  std::ifstream infile(argv[1], std::ios::binary | std::ios::ate);
  std::streamsize size = infile.tellg();
  infile.seekg(0, std::ios::beg);

  std::vector<char> buffer(size);
  if (infile.read(buffer.data(), size)) {
    createISelMutator();
    LLVMFuzzerCustomMutator((uint8_t *)buffer.data(), size, size, 0);
    std::ofstream outfile =
        std::ofstream("out.bc", std::ios::out | std::ios::binary);
    outfile.write(buffer.data(), size);
    outfile.close();
  } else {
    fprintf(stderr, "I can't read the file.");
  }
  infile.close();
  return 0;
}