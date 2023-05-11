#include "matchertree.h"

#include "llvm/Support/ErrorHandling.h"
#include <stack>

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
  MatcherNode *new_node = new MatcherNode(matcher);
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

std::tuple<size_t, std::vector<bool>>
MatcherTree::getUpperBound(const std::vector<Pattern> &Patterns,
                           const std::set<size_t> &TruePredIndices) const {
  if (!root)
    return std::tuple(0, std::vector<bool>());
  std::stack<MatcherNode *> Nodes; // BFS
  std::vector<bool> ShadowMap(root->size());
  Nodes.push(root);
  size_t UpperBound = root->size();
  while (!Nodes.empty()) {
    MatcherNode *Node = Nodes.top();
    Nodes.pop();

    if (Node->hasPattern) {
      for (size_t Pred : Patterns[Node->pattern].predicates) {
        if (!TruePredIndices.count(Pred)) {
          UpperBound -= Node->size();
          for (size_t i = Node->begin; i <= Node->end; i++) {
            ShadowMap[i] = true;
          }
          break;
        }
      }
      if (Node->children.size()) {
        llvm::report_fatal_error("Found supposed leaf node with children!\n");
      }
      continue;
    }

    for (MatcherNode *ChildNode : Node->children) {
      Nodes.push(ChildNode);
    }
  }
  return std::tuple(UpperBound, ShadowMap);
}