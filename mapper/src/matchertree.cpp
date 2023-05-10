#include "matchertree.h"

bool MatcherNode::overlaps(const MatcherNode &node) const {
  // make a the left-most interval
  const MatcherNode &a = begin <= node.begin ? *this : node;
  const MatcherNode &b = begin <= node.begin ? node : *this;

  return a != b && !a.contains(b) && !b.contains(a) && a.end <= b.begin;
}

MatcherTree::MatcherTree(std::vector<Matcher> &matchers) : root(nullptr) {
  std::sort(matchers.begin(), matchers.end());
  for (const Matcher &matcher : matchers) {
    insert(matcher);
  }
}

void MatcherTree::insert(const Matcher &matcher) {
  MatcherNode *new_node = new MatcherNode(matcher.pattern, matcher.index,
                                          matcher.index + matcher.size - 1);
  if (!root) {
    root = new_node;
    return;
  }
  MatcherNode *node = root;
  while (node->children.size() > 0) {
    bool child_found = false;
    for (MatcherNode *child : node->children) {
      if (child->contains(*new_node)) {
        node = child;
        child_found = true;
        break;
      }
    }
    if (!child_found)
      break;
  }
  node->children.insert(new_node);
}

std::set<size_t> MatcherTree::getPatternsAt(size_t i) const {
  std::set<size_t> patterns;
  if (!root || !root->contains(i)) {
    return patterns;
  }
  MatcherNode *node = root;
  while (node->children.size()) {
    patterns.insert(node->pattern);
    bool found_child = false;
    for (MatcherNode *child : node->children) {
      if (child->contains(i)) {
        node = child;
        found_child = true;
        break;
      }
    }
    if (!found_child)
      break;
  }
  return patterns;
}