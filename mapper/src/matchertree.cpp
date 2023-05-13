#include "matchertree.h"
#include "lookup.h"

#include "llvm/Support/raw_ostream.h"

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

template <typename A, typename B>
std::pair<B, A> FlipPair(const std::pair<A, B> &p) {
  return std::pair<B, A>(p.second, p.first);
}

// https://stackoverflow.com/questions/5056645/sorting-stdmap-using-value
// flips an associative container of A,B pairs to B,A pairs
template <typename A, typename B, template <class, class, class...> class M,
          class... Args>
std::multimap<B, A> FlipMap(const M<A, B, Args...> &Src) {
  std::multimap<B, A> Dst;
  std::transform(Src.begin(), Src.end(), std::inserter(Dst, Dst.begin()),
                 FlipPair<A, B>);
  return Dst;
}

std::tuple<size_t, std::vector<bool>, std::multimap<size_t, size_t>>
MatcherTree::getUpperBound() const {
  if (!Root)
    return std::tuple(0, std::vector<bool>(), std::multimap<size_t, size_t>());
  std::vector<bool> ShadowMap(Root->Size());
  size_t UpperBound = Root->Size();
  std::unordered_map<size_t, size_t> BlameMap;
  visit(Root, UpperBound, ShadowMap, BlameMap);
  return std::tuple(UpperBound, ShadowMap, FlipMap(BlameMap));
}

void MatcherTree::visit(MatcherNode *N, size_t &UpperBound,
                        std::vector<bool> &ShadowMap,
                        std::unordered_map<size_t, size_t> &BlameMap) const {
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

  size_t FailedIndex = N->End + 1;
  for (MatcherNode *C : N->Children) {
#ifndef NDEBUG
    if (C->Kind == Matcher::CheckPatternPredicate &&
        LT.PK.pat(C->PatPredIdx.value())->satisfied()) {
      dbgs() << "Passed pattern predicate check " << C->PatPredIdx.value()
             << " at " << C->Begin << ".\n";
    }
#endif
    if (C->Kind == Matcher::CheckPatternPredicate &&
        !LT.PK.pat(C->PatPredIdx.value())->satisfied()) {
      FailedIndex = C->End + 1;
#ifndef NDEBUG
      dbgs() << "Failed pattern predicate check " << C->PatPredIdx.value()
             << " at " << C->Begin << " (-" << (N->End - FailedIndex + 1)
             << ").\n";
#endif
      BlameMap[C->PatPredIdx.value()] += N->End - FailedIndex + 1;
      // CheckPatternPredicate is accessed, but not the rest
      UpperBound -= N->End - FailedIndex + 1;
      for (size_t I = FailedIndex; I <= N->End; I++) {
#ifndef NDEBUG
        if (ShadowMap[I])
          errs() << "Shadow map at i=" << I << " is already set to true.\n";
#endif
        ShadowMap[I] = true;
      }
      break;
    }
    visit(C, UpperBound, ShadowMap, BlameMap);
  }
}