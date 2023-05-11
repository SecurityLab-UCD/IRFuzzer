#include "matchertree.h"

#include "llvm/Support/ErrorHandling.h"
#include <stack>

bool MatcherNode::Overlaps(const MatcherNode &N) const {
  // make a the left-most interval
  const MatcherNode &A = Begin <= N.Begin ? *this : N;
  const MatcherNode &B = Begin <= N.Begin ? N : *this;

  return A != B && !A.Contains(B) && !B.Contains(A) && A.End <= B.Begin;
}

MatcherTree::MatcherTree(std::vector<Matcher> &Matchers) : Root(nullptr) {
  std::sort(Matchers.begin(), Matchers.end());
  for (const Matcher &M : Matchers) {
    insert(M);
  }
}

void MatcherTree::insert(const Matcher &M) {
  MatcherNode *NewNode = new MatcherNode(M);
  if (!Root) {
    Root = NewNode;
    return;
  }
  MatcherNode *Node = Root;
  while (Node->Children.size() > 0) {
    bool isChildFound = false;
    for (MatcherNode *Child : Node->Children) {
      if (Child->Contains(*NewNode)) {
        Node = Child;
        isChildFound = true;
        break;
      }
    }
    if (!isChildFound)
      break;
  }
  Node->Children.insert(NewNode);
}

std::tuple<size_t, std::vector<bool>>
MatcherTree::getUpperBound(const std::vector<Pattern> &Patterns,
                           const std::set<size_t> &TruePredIndices) const {
  if (!Root)
    return std::tuple(0, std::vector<bool>());
  std::stack<MatcherNode *> Nodes; // DFS
  std::vector<bool> ShadowMap(Root->Size());
  Nodes.push(Root);
  size_t UpperBound = Root->Size();
  while (!Nodes.empty()) {
    MatcherNode *Node = Nodes.top();
    Nodes.pop();

    if (Node->hasPattern) {
      for (size_t Pred : Patterns[Node->PatternIdx].Predicates) {
        if (!TruePredIndices.count(Pred)) {
          UpperBound -= Node->Size();
          for (size_t i = Node->Begin; i <= Node->End; i++) {
            ShadowMap[i] = true;
          }
          break;
        }
      }
      if (Node->Children.size()) {
        llvm::report_fatal_error("Found supposed leaf node with children!\n");
      }
      continue;
    }

    for (MatcherNode *ChildNode : Node->Children) {
      Nodes.push(ChildNode);
    }
  }
  return std::tuple(UpperBound, ShadowMap);
}