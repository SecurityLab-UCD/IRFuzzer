#pragma once
#ifndef SHADOW_MAP_H_
#define SHADOW_MAP_H_
#include "llvm/Support/raw_ostream.h"
#include <functional>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

std::vector<bool> readShadowMap(size_t MapBitSize, const std::string &FileName);

std::vector<std::vector<bool>>
readShadowMaps(size_t MapBitSize, const std::vector<std::string> &FileNames);

bool writeShadowMap(const std::vector<bool> &Map, const std::string &FileName);

std::vector<bool> doMapOp(const std::vector<std::vector<bool>> &Maps,
                          std::function<bool(bool, bool)> Op);

class MapStatPrinter {
  typedef std::tuple<std::string, std::string, size_t, size_t> StatTy;

  std::vector<StatTy> Stats;
  size_t MaxFilenameLen = 0;
  size_t MaxTableSize = 0;
  std::string Description = "";
  size_t MaxDescLen = 0;
  mutable size_t Limit = std::numeric_limits<size_t>::max();

public:
  enum SortTy { None, Asc, Desc };

  // Set row description
  MapStatPrinter(const std::string &Description)
      : Description(Description), MaxDescLen(Description.size()) {}
  // No description
  MapStatPrinter() = default;

  static inline size_t getIdxCovered(const std::vector<bool> &Map) {
    return std::count(Map.begin(), Map.end(), false);
  }

  std::string format(const StatTy &Stat) const;
  void print() const;

  void addFile(const std::string &Filename, const std::vector<bool> &Map);
  void addFile(const std::string &Filename, size_t Covered, size_t TableSize);
  void addFile(const std::string &Filename, size_t TableSize);
  void addStat(size_t Covered, size_t TableSize);
  void addStat(const std::string &Desc, size_t Covered, size_t TableSize);
  void addMap(const std::vector<bool> &Map);
  void summarize(const std::string &Desc, size_t Covered, size_t TableSize,
                 bool alignToDesc = false);
  void summarize(const std::string &Desc, const std::vector<bool> &Map,
                 bool alignToDesc = false);

  void asc();
  void desc();
  void sort(SortTy S);

  void limit(size_t L);
  bool atLimit() const;

private:
  void addStat(const std::string &Filename, const std::string &Desc,
               size_t Covered, size_t TableSize);
};

#endif // SHADOW_MAP_H_