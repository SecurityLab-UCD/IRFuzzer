#pragma once
#ifndef MATCHER_TREE_H_
#define MATCHER_TREE_H_
#include "lookup.h"
#include "llvm/ADT/SmallSet.h"
#include <set>
#include <string>

class MatcherTree;

// Node in a matcher tree, which holds a pattern index
class MatcherNode {
  friend class MatcherTree;

  struct PtrCmp {
    bool operator()(const MatcherNode *L, const MatcherNode *R) const {
      return L->Begin < R->Begin || (L->Begin == R->Begin && L->End > R->End);
    }
  };

  size_t PatternIdx; // Corresponding pattern index if it's a leaf node
  // The node represents a closed interval [Begin, End]
  size_t Begin;
  size_t End;
  // A leaf node is a node that is related to a pattern.
  // NOTE: Currently not sure if nodes without a child are always leaves.
  bool hasPattern;
  std::set<MatcherNode *, PtrCmp> Children;

public:
  MatcherNode(const Matcher &M)
      : PatternIdx(M.PatternIdx), Begin(M.Idx), End(M.Idx + M.Size - 1),
        hasPattern(M.hasPattern()) {}
  MatcherNode(const MatcherNode &) = delete;
  MatcherNode(MatcherNode &&) = delete;

  ~MatcherNode() {
    for (MatcherNode *Child : Children) {
      delete Child;
    }
  }

  size_t Size() const { return End - Begin + 1; }

  bool Contains(size_t i) const { return Begin <= i && i <= End; }

  bool Contains(const MatcherNode &N) const {
    return Begin <= N.Begin && N.End <= End;
  }

  bool operator==(const MatcherNode &N) const {
    return Begin == N.Begin && End == N.End;
  }

  bool operator!=(const MatcherNode &N) const { return !(*this == N); }

  /// Returns true if the intervals only overlap (but not contained within
  /// another or identical)
  bool Overlaps(const MatcherNode &N) const;
};

class MatcherTree {
private:
  MatcherNode *Root;

  void insert(const Matcher &matcher);

public:
  /// @brief Sorts the matcher list by index and populates the tree
  /// @param Matchers list of matchers
  MatcherTree(std::vector<Matcher> &Matchers);
  MatcherTree() = delete;
  MatcherTree(const MatcherTree &) = delete;
  MatcherTree(MatcherTree &&MT) : Root(MT.Root) { MT.Root = nullptr; }

  ~MatcherTree() {
    if (Root)
      delete Root;
  }

  std::tuple<size_t, std::vector<bool>>
  getUpperBound(const std::vector<Pattern> &Patterns,
                const std::set<size_t> &TruePredIndices) const;
};
#endif // MATCHER_TREE_H_
