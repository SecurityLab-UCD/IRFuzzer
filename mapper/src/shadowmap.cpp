#include "shadowmap.h"
#include <climits>
#include <fstream>

std::vector<bool> readShadowMap(size_t MapBitSize,
                                const std::string &FileName) {
  std::ifstream ShadowMapIfs(FileName, std::ios::binary);
  std::vector<bool> ShadowMap;
  ShadowMap.reserve(MapBitSize);

  char data;
  while (ShadowMapIfs.read(&data, 1)) {
    for (int i = 0; i < CHAR_BIT && ShadowMap.size() < MapBitSize; i++) {
      // NOTE: 1 bit in shadow map means MT index not covered; reading as is
      // here
      ShadowMap.push_back(((data >> i) & 1) != 0);
    }
  }
  return ShadowMap;
}

bool writeShadowMap(const std::vector<bool> &Map, const std::string &FileName) {
  std::ofstream ShadowMapOfs(FileName, std::ios::binary);
  if (!ShadowMapOfs)
    return false;
  unsigned char bytebuf = 0;
  for (size_t i = 0; i < Map.size(); i++) {
    if (i % 8 == 0 && i) {
      ShadowMapOfs << bytebuf;
      bytebuf = 0;
    }
    if (!Map[i])
      continue;
    bytebuf |= 1 << (7 - (i % 8));
  }
  if (Map.size() % 8 != 0) {
    ShadowMapOfs << bytebuf;
  }
  return ShadowMapOfs.good();
}

size_t getCoveredIndices(const std::vector<bool> &Map) {
  return std::count(Map.begin(), Map.end(), false);
}

void printShadowMapStats(std::vector<bool> &ShadowMap,
                         const std::string &Description,
                         const std::string &FileName) {
  printShadowMapStats(getCoveredIndices(ShadowMap), ShadowMap.size(),
                      Description, FileName);
}

void printShadowMapStats(size_t CoveredIndices, size_t MapSize,
                         const std::string &Description,
                         const std::string &FileName) {
  double Coverage = CoveredIndices;
  Coverage /= MapSize;
  Coverage *= 100;

  if (!FileName.empty()) {
    llvm::outs() << "[" << FileName << "] ";
  }
  llvm::outs() << Description << ": " << CoveredIndices << " out of " << MapSize
               << " (" << std::to_string(Coverage) << "%)";
  llvm::outs() << '\n';
}
