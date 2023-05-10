#pragma once
#ifndef SHADOW_MAP_H_
#define SHADOW_MAP_H_
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

std::vector<bool> readShadowMap(size_t MapBitSize, const std::string &FileName);

bool writeShadowMap(const std::vector<bool> &Map, const std::string &FileName);

void printShadowMapStats(const std::vector<bool> &ShadowMap,
                         const std::string &Description = "Upper bound",
                         const std::string &FileName = "");

#endif // SHADOW_MAP_H_