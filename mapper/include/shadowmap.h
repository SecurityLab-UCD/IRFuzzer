#pragma once
#ifndef SHADOW_MAP_H_
#define SHADOW_MAP_H_
#include <functional>
#include <string>
#include <vector>

std::vector<bool> readShadowMap(size_t MapBitSize, const std::string &FileName);

bool writeShadowMap(const std::vector<bool> &Map, const std::string &FileName);

size_t getCoveredIndices(const std::vector<bool> &Map);

void printShadowMapStats(const std::vector<bool> &ShadowMap,
                         const std::string &Description,
                         const std::string &FileName = "");

void printShadowMapStats(size_t CoveredIndices, size_t MapSize,
                         const std::string &Description,
                         const std::string &FileName = "");

template <typename ConstIterator>
std::vector<bool> doMapOp(size_t TableSize, ConstIterator MapFilesBegin,
                          ConstIterator MapFilesEnd,
                          std::function<bool(bool, bool)> Op) {
  std::vector<std::vector<bool>> Maps;
  for (auto It = MapFilesBegin; It != MapFilesEnd; It++) {
    const std::string &MapFile = *It;
    Maps.push_back(readShadowMap(TableSize, MapFile));
    printShadowMapStats(*Maps.rbegin(), "", MapFile);
  }
  std::vector<bool> ResultMap(Maps[0]);
  for (size_t M = 1; M < Maps.size(); M++) {
    for (size_t I = 0; I < TableSize; I++) {
      ResultMap[I] = Op(ResultMap[I], Maps[M][I]);
    }
  }
  return ResultMap;
}

#endif // SHADOW_MAP_H_