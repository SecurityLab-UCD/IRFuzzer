#pragma once
#ifndef SEGMENT_TREE_H_
#define SEGMENT_TREE_H_
#include "lookup.h"
#include "llvm/ADT/SmallSet.h"
#include <set>
#include <string>

class MatcherTree;

// Node in a matcher tree, which holds a pattern index
class MatcherNode {
  friend class MatcherTree;

  struct PtrCmp {
    inline bool operator()(const MatcherNode *lhs,
                           const MatcherNode *rhs) const {
      return lhs->begin < rhs->begin ||
             (lhs->begin == rhs->begin && lhs->end > rhs->end);
    }
  };

  size_t pattern; // Corresponding pattern index if it's a leaf node
  // The node represents a closed interval [begin, end]
  size_t begin;
  size_t end;
  // A leaf node is a node that is related to a pattern.
  // NOTE: Currently not sure if nodes without a child are always leaves.
  bool hasPattern;
  std::set<MatcherNode *, PtrCmp> children;

public:
  inline MatcherNode(const Matcher &M)
      : pattern(M.pattern), begin(M.index), end(M.index + M.size - 1),
        hasPattern(M.hasPattern()) {}
  MatcherNode(const MatcherNode &) = delete;
  MatcherNode(MatcherNode &&) = delete;

  inline ~MatcherNode() {
    for (MatcherNode *child : children) {
      delete child;
    }
  }

  inline size_t size() const { return end - begin + 1; }

  inline bool contains(size_t i) const { return begin <= i && i <= end; }

  inline bool contains(const MatcherNode &node) const {
    return begin <= node.begin && node.end <= end;
  }

  inline bool operator==(const MatcherNode &rhs) const {
    return begin == rhs.begin && end == rhs.end;
  }

  inline bool operator!=(const MatcherNode &rhs) const {
    return !(*this == rhs);
  }

  /// Returns true if the intervals only overlap (but not contained within
  /// another or identical)
  bool overlaps(const MatcherNode &node) const;
};

class MatcherTree {
private:
  MatcherNode *root;

  void insert(const Matcher &matcher);

public:
  /// @brief Sorts the matcher list by index and populates the tree
  /// @param matchers list of matchers
  MatcherTree(std::vector<Matcher> &matchers);
  MatcherTree() = delete;
  MatcherTree(const MatcherTree &) = delete;
  MatcherTree(MatcherTree &&mt) : root(mt.root) { mt.root = nullptr; }

  inline ~MatcherTree() {
    if (root)
      delete root;
  }

  std::tuple<size_t, std::vector<bool>>
  getUpperBound(const std::vector<Pattern> &Patterns,
                const std::set<size_t> &TruePredIndices) const;
};
#endif // SEGMENT_TREE_H_
