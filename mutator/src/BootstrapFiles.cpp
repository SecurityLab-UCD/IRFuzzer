#include "BootstrapFiles.h"

#include "llvm/Support/Debug.h"
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>

BootstrapFiles::BootstrapFiles(unsigned N) : N(N) {
  namespace fs = std::filesystem;
  const char *BootEnv = std::getenv("BOOT_DIR");
  if (BootEnv == nullptr) {
    std::cerr << "Environment variable BOOT_DIR is not set." << std::endl;
    return;
  }

  fs::path BootPath(BootEnv);
  // Check if the directory exists
  if (!fs::exists(BootPath) || !fs::is_directory(BootPath)) {
    std::cerr << "Path is not a valid directory: " << BootPath << std::endl;
    return;
  }

  // Iterate through the directory and print file names
  for (const auto &entry : fs::directory_iterator(BootPath)) {
    auto FilePath = entry.path();
    std::ifstream file(FilePath, std::ios::binary);
    if (!file) {
      std::cerr << "Can't open this file\n";
      continue;
    }
    // Get the size of the file
    file.seekg(0, std::ios::end);
    std::streamsize Size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read the file into a vector of bytes
    std::vector<char> buffer(Size);
    if (!file.read(buffer.data(), Size)) {
      std::cerr << "Error reading file: " << FilePath << std::endl;
    }

    // Close the file
    file.close();
    std::cerr << "Got " << entry << "\n";

    files.push_back(buffer);
  }
  std::cerr << "BootstrapFiles: I have " << files.size()
            << " files in total.\n";
}
