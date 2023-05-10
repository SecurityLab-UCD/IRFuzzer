#pragma once
#ifndef SEGMENT_TREE_H_
#define SEGMENT_TREE_H_
#include "lookup.h"
#include "llvm/ADT/SmallSet.h"
#include <cstddef>
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

  size_t pattern;
  size_t begin;
  size_t end;
  std::set<MatcherNode *, PtrCmp> children;

public:
  inline MatcherNode(size_t pattern, size_t begin, size_t end)
      : pattern(pattern), begin(begin), end(end) {}
  MatcherNode(const MatcherNode &) = delete;
  MatcherNode(MatcherNode &&) = delete;

  inline ~MatcherNode() {
    for (MatcherNode *child : children) {
      delete child;
    }
  }

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

  std::set<size_t> getPatternsAt(size_t i) const;
};
#endif // SEGMENT_TREE_H_
