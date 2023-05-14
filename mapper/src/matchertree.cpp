#include "matchertree.h"
#include "lookup.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

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
  if (MT.empty())
    return std::tuple(0, std::vector<bool>(),
                      std::multimap<size_t, size_t, std::greater<size_t>>());
  std::vector<bool> ShadowMap(MT[0].size());
  size_t UpperBound = ShadowMap.size();
  std::unordered_map<size_t, size_t> BlameMap;
  size_t I = 0;
  visit(I, UpperBound, ShadowMap, BlameMap);
  return std::tuple(UpperBound, ShadowMap, FlipMap(BlameMap));
}

/// @brief Visit a matcher tree node
/// @param I current matcher tree index
/// @param UpperBound Current upper bound value
/// @param ShadowMap Shadow map
/// @param BlameMap Pattern predicate blame list (pat pred -> loss)
/// @return if this leaf failed a pattern predicate check (not named predicate)
bool MatcherTree::visit(size_t &I, size_t &UpperBound,
                        std::vector<bool> &ShadowMap,
                        std::unordered_map<size_t, size_t> &BlameMap) const {
  if (MT[I].isLeaf()) {
    // We only care about leaves with a pattern or pattern predicate index
    if (MT[I].hasPattern() && LT.PK.Verbosity) {
      const Pattern &Pat = LT.Patterns[MT[I].PIdx];
      // Verify that all named predicates are satisfied.
      for (size_t Pred : Pat.NamedPredicates) {
        if (!LT.PK.name(Pred)->satisfied()) {
          // In certain cases, some matchers have the same TableGen pattern but
          // different predicates, and the one with a different predicate is not
          // captured. They don't really cause an issue for the calculation, so
          // we just emit an error message and move on.
          errs()
              << "ERROR: Failed named predicate check " << Pred << " at "
              << MT[I].Begin << ".\n"
              << "ERROR: Reached leaf when named predicate is unsatisfied!\n";
        }
      }
    } else if (MT[I].hasPatPred()) {
      if (!LT.PK.pat(MT[I].PIdx)->satisfied()) {
        I++;
        return true;
      } else if (LT.PK.Verbosity > 2) {
        dbgs() << "DEBUG: Passed pattern predicate check " << MT[I].PIdx
               << " at " << MT[I].Begin << ".\n";
      }
    }
    I++;
    return false;
  }

  // We have a switch, scope, or a child (i.e. switch or scope case)
  size_t PI = I; // parent index
  I++;
  bool Failed = false;
  for (; I < MT.size() && MT[PI].contains(MT[I]);) {
    if (!Failed) {
      Failed = visit(I, UpperBound, ShadowMap, BlameMap);
    } else { // Previous pattern predicate check failed
      size_t Loss = MT[PI].End - MT[I].Begin + 1;
      BlameMap[MT[I].PIdx] += Loss;
      UpperBound -= Loss;

      if (LT.PK.Verbosity > 2)
        errs() << "DEBUG: Failed pattern predicate check " << MT[I - 1].PIdx
               << " at " << MT[I - 1].Begin << " (-" << Loss << ").\n";

      std::fill(ShadowMap.begin() + MT[I].Begin,
                ShadowMap.begin() + MT[PI].End + 1, true);

      // Fast-forward out of the parent
      while (I < MT.size() && MT[PI].contains(MT[I]))
        I++;
      return false;
    }
  }
  return false;
}