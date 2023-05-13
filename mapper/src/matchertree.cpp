#include "matchertree.h"
#include "lookup.h"

#include "llvm/Support/raw_ostream.h"
#include <stack>

using namespace llvm;

bool MatcherNode::Overlaps(const MatcherNode &N) const {
  // make a the left-most interval
  const MatcherNode &A = Begin <= N.Begin ? *this : N;
  const MatcherNode &B = Begin <= N.Begin ? N : *this;

  return A != B && !A.Contains(B) && !B.Contains(A) && A.End <= B.Begin;
}

MatcherTree::MatcherTree(const LookupTable &_LT) : Root(nullptr), LT(_LT) {
  for (const Matcher &M : LT.Matchers) {
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
  if (!Node->Contains(*NewNode)) {
    llvm::report_fatal_error("Insertion interval does not encompass new node.");
  }
  Node->Children.insert(NewNode);
}

std::tuple<size_t, std::vector<bool>> MatcherTree::getUpperBound() const {
  if (!Root)
    return std::tuple(0, std::vector<bool>());
  std::stack<MatcherNode *> Nodes; // DFS
  std::vector<bool> ShadowMap(Root->Size());
  Nodes.push(Root);
  size_t UpperBound = Root->Size();
  visit(Root, UpperBound, ShadowMap);
  return std::tuple(UpperBound, ShadowMap);
}

void MatcherTree::visit(MatcherNode *N, size_t &UpperBound,
                        std::vector<bool> &ShadowMap) const {
  if (N->Children.size() == 0) {
    if (N->Kind != Matcher::CompleteMatch && N->Kind != Matcher::MorphNodeTo)
      return;
    const Pattern &Pat = LT.Patterns[N->PatternIdx.value()];
    for (size_t Pred : Pat.NamedPredicates) {
      if (!LT.PK.name(Pred)->satisfied()) {
        // Supposedly this named predicate was implicitly checked through the
        // pattern predicate. This is bad.
        errs() << "Failed named predicate check " << Pred << ".\n";
        errs() << "Traversed to leaf with unsatisfied pattern predicate.\n";
        exit(1);
      }
    }
    return;
  }

  bool FailedPredicate = false;
  for (MatcherNode *C : N->Children) {
#ifndef NDEBUG
    if (!FailedPredicate && C->Kind == Matcher::CheckPatternPredicate &&
        LT.PK.pat(C->PatPredIdx.value())->satisfied()) {
      errs() << "Passed pattern predicate check " << C->PatPredIdx.value()
             << ".\n";
    }
#endif
    if (!FailedPredicate && C->Kind == Matcher::CheckPatternPredicate &&
        !LT.PK.pat(C->PatPredIdx.value())->satisfied()) {
#ifndef NDEBUG
      errs() << "Failed pattern predicate check " << C->PatPredIdx.value()
             << ".\n";
#endif
      FailedPredicate = true;
      // CheckPatternPredicate node is accessed, but not the rest
      continue;
    } else if (FailedPredicate) {
      // Mark the rest as failed
      UpperBound -= C->Size();
      for (size_t I = C->Begin; I <= C->End; I++)
        ShadowMap[I] = true;
    } else {
      visit(C, UpperBound, ShadowMap);
    }
  }
}