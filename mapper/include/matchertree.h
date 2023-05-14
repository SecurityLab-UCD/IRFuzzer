#pragma once
#ifndef MATCHER_TREE_H_
#define MATCHER_TREE_H_
#include "lookup.h"
#include "llvm/ADT/SmallSet.h"
#include <map>
#include <set>
#include <string>

struct LookupTable;

class MatcherTree {
private:
  const LookupTable &LT;
  // The Matchers vector in lookup table for easy access
  const std::vector<Matcher> &MT;

public:
  MatcherTree(const LookupTable &_LT) : LT(_LT), MT(_LT.Matchers) {}
  MatcherTree() = delete;
  MatcherTree(const MatcherTree &) = delete;
  MatcherTree(MatcherTree &&M) : LT(M.LT), MT(M.MT) {}

  /// @brief Calculate matcher table coverage upper bound
  /// @return (covered indices, shadow map, coverage loss -> pat pred idx)
  std::tuple<size_t, std::vector<bool>,
             std::multimap<size_t, size_t, std::greater<size_t>>>
  getUpperBound() const;

private:
  bool visit(size_t &I, size_t &UpperBound, std::vector<bool> &ShadowMap,
             std::unordered_map<size_t, size_t> &BlameMap) const;
};
#endif // MATCHER_TREE_H_
