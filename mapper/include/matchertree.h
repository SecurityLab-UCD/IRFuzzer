#pragma once
#ifndef MATCHER_TREE_H_
#define MATCHER_TREE_H_
#include "lookup.h"
#include "llvm/ADT/SmallSet.h"
#include <map>
#include <set>
#include <string>

class MatcherTree;

// Node in a matcher tree, which holds a pattern index
class MatcherNode {
  friend class MatcherTree;

  const Matcher &M;

  std::vector<MatcherNode *> Children;

public:
  MatcherNode(const Matcher &M) : M(M) {}
  MatcherNode(const MatcherNode &) = delete;
  MatcherNode(MatcherNode &&) = delete;

  ~MatcherNode() {
    for (MatcherNode *Child : Children) {
      delete Child;
    }
  }

  bool Contains(size_t i) const { return M.Begin <= i && i <= M.End; }

  bool Contains(const MatcherNode &N) const {
    return M.Begin <= N.M.Begin && N.M.End <= M.End;
  }

  bool operator==(const MatcherNode &N) const {
    return M.Begin == N.M.Begin && M.End == N.M.End;
  }

  bool operator!=(const MatcherNode &N) const { return !(*this == N); }

  /// Returns true if the intervals only overlap (but not contained within
  /// another or identical)
  bool Overlaps(const MatcherNode &N) const;
};

struct LookupTable;

class MatcherTree {
private:
  MatcherNode *Root;
  const LookupTable &LT;

  void insert(const Matcher &matcher);

public:
  /// @brief Sorts the matcher list by index and populates the tree
  /// @param Matchers list of matchers
  MatcherTree(const LookupTable &_LT);
  MatcherTree() = delete;
  MatcherTree(const MatcherTree &) = delete;
  MatcherTree(MatcherTree &&MT) : Root(MT.Root), LT(MT.LT) {
    MT.Root = nullptr;
  }

  ~MatcherTree() {
    if (Root)
      delete Root;
  }

  /// @brief Calculate matcher table coverage upper bound
  /// @return (covered indices, shadow map, coverage loss -> pat pred idx)
  std::tuple<size_t, std::vector<bool>,
             std::multimap<size_t, size_t, std::greater<size_t>>>
  getUpperBound() const;

private:
  void visit(MatcherNode *N, size_t &UpperBound, std::vector<bool> &ShadowMap,
             std::unordered_map<size_t, size_t> &BlameMap) const;
};
#endif // MATCHER_TREE_H_
