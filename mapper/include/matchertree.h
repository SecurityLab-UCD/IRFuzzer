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

  std::optional<size_t> PatternIdx; // pattern index if it's a leaf node
  std::optional<size_t> PatPredIdx; // pattern predicate if it's a check node

  // The node represents a closed interval [Begin, End]
  size_t Begin;
  size_t End;

  Matcher::KindTy Kind;

  // A leaf node is a node that is related to a pattern.
  // NOTE: Currently not sure if nodes without a child are always leaves.
  std::set<MatcherNode *, PtrCmp> Children;

public:
  MatcherNode(const Matcher &M)
      : PatternIdx(M.PatternIdx), PatPredIdx(M.PatPredIdx), Begin(M.Idx),
        End(M.Idx + M.Size - 1), Kind(M.Kind) {}
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

  std::tuple<size_t, std::vector<bool>> getUpperBound() const;

private:
  void visit(MatcherNode *N, size_t &UpperBound,
             std::vector<bool> &ShadowMap) const;
};
#endif // MATCHER_TREE_H_
