#include "matchertree.h"
#include "lookup.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

template <typename A, typename B, template <class, class, class...> class M,
          class... Args>
std::vector<std::pair<A, B>> getSortedVecFromMap(const M<A, B, Args...> &Src) {
  std::vector<std::pair<A, B>> Dst(Src.size());
  std::copy(Src.begin(), Src.end(), Dst.begin());
  std::sort(Dst.begin(), Dst.end(),
            [](const auto &L, const auto &R) { return L.second > R.second; });
  return Dst;
}

std::tuple<size_t, std::vector<bool>, PatPredBlameList>
MatcherTree::getUpperBound() const {
  if (MT.empty())
    return std::tuple(0, std::vector<bool>(), PatPredBlameList());
  std::vector<bool> ShadowMap(MT[0].size());
  size_t UpperBound = ShadowMap.size();
  std::unordered_map<size_t, size_t> BlameMap;
  size_t I = 0;
  getUpperBound(I, UpperBound, ShadowMap, BlameMap);
  return std::tuple(UpperBound, ShadowMap, getSortedVecFromMap(BlameMap));
}

/// @brief Visit a matcher tree node and calculate coverage upper bound
/// @param I current matcher tree index
/// @param UpperBound Current upper bound value
/// @param ShadowMap Shadow map
/// @param BlameMap Pattern predicate blame list (pat pred -> loss)
/// @return if this leaf failed a pattern predicate check (not named predicate)
bool MatcherTree::getUpperBound(
    size_t &I, size_t &UpperBound, std::vector<bool> &ShadowMap,
    std::unordered_map<size_t, size_t> &BlameMap) const {
  if (MT[I].isLeaf()) {
    // We only care about leaves with a pattern or pattern predicate index
    if (MT[I].hasPattern() && LT.PK.Verbosity &&
        !LT.PK.CustomizedPatternPredicates) {
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
      Failed = getUpperBound(I, UpperBound, ShadowMap, BlameMap);
    } else { // Previous pattern predicate check failed
      size_t Loss = MT[PI].End - MT[I].Begin + 1;
      BlameMap[MT[I - 1].PIdx] += Loss;
      UpperBound -= Loss;

      if (LT.PK.Verbosity > 2)
        errs() << "DEBUG: Failed pattern predicate check " << MT[I - 1].PIdx
               << " at " << MT[I - 1].Begin << " with parent kind "
               << MT[PI].getKindAsString() << " (-" << Loss << ").\n";

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

std::tuple<MatcherBlameList, PatPredBlameList>
MatcherTree::analyzeMap(const std::vector<bool> &ShadowMap) {
  if (MT.empty())
    return std::tuple(MatcherBlameList(), PatPredBlameList());
  size_t I = 0;
  std::unordered_map<Matcher::KindTy, size_t> MatcherBlameMap;
  std::unordered_map<size_t, size_t> PatPredBlameMap;
  analyzeMap(I, ShadowMap, MatcherBlameMap, PatPredBlameMap);
  return std::tuple(getSortedVecFromMap(MatcherBlameMap),
                    getSortedVecFromMap(PatPredBlameMap));
}

bool MatcherTree::analyzeMap(
    size_t &I, const std::vector<bool> &ShadowMap,
    std::unordered_map<Matcher::KindTy, size_t> &MatcherBlame,
    std::unordered_map<size_t, size_t> &PatPredBlame) {
  if (MT[I].isLeaf()) {
    return ShadowMap[MT[I++].Begin];
  } else if (ShadowMap[MT[I].Begin]) {
    // A scope or group wasn't covered after a OPC_CheckPatternPredicate failed.
    I++;
    return true;
  }

  size_t PI = I; // parent index
  I++;
  for (; I < MT.size() && MT[PI].contains(MT[I]);) {
    if (!analyzeMap(I, ShadowMap, MatcherBlame, PatPredBlame)) {
      continue; // Matcher is covered. Keep going.
    }
    I--; // Move to the first non-covered matcher
    size_t Loss = MT[PI].End - MT[I].Begin + 1;
    I--; // Move to the matcher that failed

    if (MT[I].hasPatPred())
      PatPredBlame[MT[I].PIdx] += Loss;
    if (LT.PK.Verbosity > 2) {
      if (MT[I].hasPatPred()) {
        errs() << "DEBUG: Failed pattern predicate check " << MT[I].PIdx
               << " at " << MT[I].Begin << " with parent kind "
               << MT[PI].getKindAsString() << " (-" << Loss << ").\n";
      } else {
        errs() << "DEBUG: Failed to match " << MT[I].getKindAsString() << " at "
               << MT[I].Begin << " (-" << Loss << ").\n";
      }
    }
    MatcherBlame[MT[I].Kind] += Loss;
    // TODO: Any other information we can record?

    // TODO: Maybe add some debug checks? Like confirming that the previous
    // matcher / rest of the matcher scope actually didn't get covered at all?

    // Fast-forward out of the parent
    while (I < MT.size() && MT[PI].contains(MT[I]))
      I++;
    return false;
  }
  return false;
}
