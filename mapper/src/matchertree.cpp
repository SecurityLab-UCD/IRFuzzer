#include "matchertree.h"
#include "lookup.h"

#include "llvm/Support/raw_ostream.h"

using namespace llvm;

bool MatcherNode::Overlaps(const MatcherNode &N) const {
  // make a the left-most interval
  const MatcherNode &A = M.Begin <= N.M.Begin ? *this : N;
  const MatcherNode &B = M.Begin <= N.M.Begin ? N : *this;

  return A != B && !A.Contains(B) && !B.Contains(A) && A.M.End <= B.M.Begin;
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
  Node->Children.push_back(NewNode);
}

template <typename A, typename B>
std::pair<B, A> FlipPair(const std::pair<A, B> &p) {
  return std::pair<B, A>(p.second, p.first);
}

// https://stackoverflow.com/questions/5056645/sorting-stdmap-using-value
// flips an associative container of A,B pairs to B,A pairs
template <typename A, typename B, template <class, class, class...> class M,
          class... Args>
std::multimap<B, A, std::greater<B>> FlipMap(const M<A, B, Args...> &Src) {
  std::multimap<B, A, std::greater<B>> Dst;
  std::transform(Src.begin(), Src.end(), std::inserter(Dst, Dst.begin()),
                 FlipPair<A, B>);
  return Dst;
}

std::tuple<size_t, std::vector<bool>,
           std::multimap<size_t, size_t, std::greater<size_t>>>
MatcherTree::getUpperBound() const {
  if (!Root)
    return std::tuple(0, std::vector<bool>(),
                      std::multimap<size_t, size_t, std::greater<size_t>>());
  std::vector<bool> ShadowMap(Root->M.Size());
  size_t UpperBound = Root->M.Size();
  std::unordered_map<size_t, size_t> BlameMap;
  visit(Root, UpperBound, ShadowMap, BlameMap);
  return std::tuple(UpperBound, ShadowMap, FlipMap(BlameMap));
}

void MatcherTree::visit(MatcherNode *N, size_t &UpperBound,
                        std::vector<bool> &ShadowMap,
                        std::unordered_map<size_t, size_t> &BlameMap) const {
  if (N->Children.size() == 0) {
    if (!LT.PK.Verbosity)
      return;
    if (!N->M.hasPattern())
      return;
    const Pattern &Pat = LT.Patterns[N->M.PIdx];
    for (size_t Pred : Pat.NamedPredicates) {
      // Supposedly this named predicate was checked as part of the
      // pattern predicate.
      if (!LT.PK.name(Pred)->satisfied()) {
        // This is bad.
        errs() << "ERROR: Failed named predicate check " << Pred << " at "
               << N->M.Begin << ".\n";
        errs() << "ERROR: Reached leaf when named predicate is unsatisfied!\n";
      }
    }
    return;
  }

  size_t FailedIndex = N->M.End + 1;
  for (MatcherNode *C : N->Children) {
    if (LT.PK.Verbosity > 2 && C->M.hasPatPred() &&
        LT.PK.pat(C->M.PIdx)->satisfied()) {
      dbgs() << "DEBUG: Passed pattern predicate check " << C->M.PIdx << " at "
             << C->M.Begin << ".\n";
    }
    if (C->M.hasPatPred() && !LT.PK.pat(C->M.PIdx)->satisfied()) {
      FailedIndex = C->M.End + 1;
      if (LT.PK.Verbosity > 2)
        errs() << "DEBUG: Failed pattern predicate check " << C->M.PIdx
               << " at " << C->M.Begin << " (-" << (N->M.End - FailedIndex + 1)
               << ").\n";
      BlameMap[C->M.PIdx] += N->M.End - FailedIndex + 1;
      // CheckPatternPredicate is accessed, but not the rest
      UpperBound -= N->M.End - FailedIndex + 1;
      for (size_t I = FailedIndex; I <= N->M.End; I++) {
        if (LT.PK.Verbosity && ShadowMap[I]) {
          dbgs() << "ERROR: Shadow map at i=" << I
                 << " is already set to true.\n";
        }
        ShadowMap[I] = true;
      }
      break;
    }
    visit(C, UpperBound, ShadowMap, BlameMap);
  }
}