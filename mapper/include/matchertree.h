#pragma once
#ifndef MATCHER_TREE_H_
#define MATCHER_TREE_H_
#include "lookup.h"
#include "llvm/ADT/SmallSet.h"
#include <map>
#include <set>
#include <string>

struct LookupTable;

typedef std::vector<std::pair<size_t, size_t>> PatPredBlameList;
typedef std::vector<std::pair<Matcher::KindTy, size_t>> MatcherBlameList;

class MatcherTree {

  const LookupTable &LT;
  // The Matchers vector in lookup table for easy access
  const std::vector<Matcher> &MT;

public:
  MatcherTree(const LookupTable &_LT) : LT(_LT), MT(_LT.Matchers) {}
  MatcherTree() = delete;
  MatcherTree(const MatcherTree &) = delete;
  MatcherTree(MatcherTree &&M) : LT(M.LT), MT(M.MT) {}

  /// @brief Calculate matcher table coverage upper bound
  /// @return (covered indices, shadow map, pat pred idx -> coverage loss)
  std::tuple<size_t, std::vector<bool>, PatPredBlameList> getUpperBound() const;

  std::tuple<MatcherBlameList, PatPredBlameList>
  analyzeMap(const std::vector<bool> &ShadowMap);

private:
  bool getUpperBound(size_t &I, size_t &UpperBound,
                     std::vector<bool> &ShadowMap,
                     std::unordered_map<size_t, size_t> &BlameMap) const;
  bool analyzeMap(size_t &I, const std::vector<bool> &ShadowMap,
                  std::unordered_map<Matcher::KindTy, size_t> &MatcherBlame,
                  std::unordered_map<size_t, size_t> &PatPredBlame);
};
#endif // MATCHER_TREE_H_
