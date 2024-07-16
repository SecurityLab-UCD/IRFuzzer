#ifndef BOOTSTRAP_FILES_H
#define BOOTSTRAP_FILES_H

#include <chrono>
#include <cstddef>
#include <vector>

struct BootstrapFiles {
  std::vector<std::vector<char>> files;
  size_t idx = 0;
  std::chrono::steady_clock::time_point begin =
      std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

  unsigned N;

  BootstrapFiles(unsigned N);

  std::vector<char> &pop() {
    // return (!empty()) ? files[idx++] : std::vector<char>();
    size_t oldIdx = idx;
    idx = (idx + 1) % files.size();
    return files[oldIdx];
  }
  bool empty() const { return (idx >= files.size()); }
  bool bootstraping() {
    if (empty())
      return false;
    end = std::chrono::steady_clock::now();
    // Calculate the elapsed time
    std::chrono::duration<double> elapsed = end - begin;
    return elapsed.count() < N;
  }
};

#endif