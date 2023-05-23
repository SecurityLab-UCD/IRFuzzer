#include "matchertree.h"
#include "simdjson.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>

using namespace llvm;
using namespace simdjson;

bool Matcher::operator<(const Matcher &M) const {
  if (*this == M)
    return !isLeaf() && M.isLeaf();
  return Begin == M.Begin ? End > M.End : Begin < M.Begin;
}

bool Matcher::contains(size_t i) const { return Begin <= i && i <= End; }

bool Matcher::contains(const Matcher &N) const {
  return Begin <= N.Begin && N.End <= End;
}

bool Matcher::operator==(const Matcher &N) const {
  return Begin == N.Begin && End == N.End;
}

bool Matcher::operator!=(const Matcher &N) const { return !(*this == N); }

bool Matcher::hasPattern() const {
  return Kind == CompleteMatch || Kind == MorphNodeTo;
}

bool Matcher::isLeaf() const {
  switch (Kind) {
  default:
    return true;
  case Scope:
  case ScopeGroup:
  case SwitchOpcode:
  case SwitchType:
  case SwitchOpcodeCase:
  case SwitchTypeCase:
    return false;
  }
}

bool Matcher::isChild() const { return isCase() || Kind == ScopeGroup; }

bool Matcher::isCase() const {
  return Kind == SwitchOpcodeCase || Kind == SwitchTypeCase;
}

bool Matcher::hasPatPred() const { return Kind == CheckPatternPredicate; }

size_t Matcher::size() const { return End - Begin + 1; }

#define ENUM_TO_STR(name)                                                      \
  case name:                                                                   \
    return #name;

std::string Matcher::getKindAsString(KindTy Kind) {
  switch (Kind) {
    ENUM_TO_STR(Scope)
    ENUM_TO_STR(RecordNode)
    ENUM_TO_STR(RecordChild)
    ENUM_TO_STR(RecordMemRef)
    ENUM_TO_STR(CaptureGlueInput)
    ENUM_TO_STR(MoveChild)
    ENUM_TO_STR(MoveParent)
    ENUM_TO_STR(CheckSame)
    ENUM_TO_STR(CheckChildSame)
    ENUM_TO_STR(CheckPatternPredicate)
    ENUM_TO_STR(CheckPredicate)
    ENUM_TO_STR(CheckOpcode)
    ENUM_TO_STR(SwitchOpcode)
    ENUM_TO_STR(CheckType)
    ENUM_TO_STR(SwitchType)
    ENUM_TO_STR(CheckChildType)
    ENUM_TO_STR(CheckInteger)
    ENUM_TO_STR(CheckChildInteger)
    ENUM_TO_STR(CheckCondCode)
    ENUM_TO_STR(CheckChild2CondCode)
    ENUM_TO_STR(CheckValueType)
    ENUM_TO_STR(CheckComplexPat)
    ENUM_TO_STR(CheckAndImm)
    ENUM_TO_STR(CheckOrImm)
    ENUM_TO_STR(CheckImmAllOnesV)
    ENUM_TO_STR(CheckImmAllZerosV)
    ENUM_TO_STR(CheckFoldableChainNode)
    ENUM_TO_STR(EmitInteger)
    ENUM_TO_STR(EmitStringInteger)
    ENUM_TO_STR(EmitRegister)
    ENUM_TO_STR(EmitConvertToTarget)
    ENUM_TO_STR(EmitMergeInputChains)
    ENUM_TO_STR(EmitCopyToReg)
    ENUM_TO_STR(EmitNode)
    ENUM_TO_STR(EmitNodeXForm)
    ENUM_TO_STR(CompleteMatch)
    ENUM_TO_STR(MorphNodeTo)
    ENUM_TO_STR(ScopeGroup)
    ENUM_TO_STR(SwitchTypeCase)
    ENUM_TO_STR(SwitchOpcodeCase)
  default:
    return "Unknown";
  }
}
#undef ENUM_TO_STR

std::string Matcher::getKindAsString() const { return getKindAsString(Kind); }

// NOTE: exits program if error encountered
std::string readFile(const std::string &Filename) {
  std::ifstream LookupIfs(Filename);
  if (!LookupIfs) {
    errs() << "Failed to open lookup file!\n";
    exit(1);
  }
  std::string LookupTableStr;
  std::getline(LookupIfs, LookupTableStr);
  if (LookupTableStr.empty()) {
    errs() << "Empty lookup table!\n";
    exit(1);
  }
  return LookupTableStr;
}

std::vector<std::string> getStringArray(ondemand::document &TableJSON,
                                        const std::string &Key) {
  std::vector<std::string> V;
  ondemand::array Arr = TableJSON[Key].get_array();
  for (auto Predicate : Arr) {
    V.push_back(std::string(Predicate.get_string().value()));
  }
  return V;
}

std::vector<Pattern> getPatterns(ondemand::document &TableJSON) {
  std::vector<Pattern> Patterns;
  for (ondemand::object PatternObject : TableJSON["patterns"]) {
    Pattern ThePattern;
    ThePattern.Index = Patterns.size();
    for (uint64_t PredIdx : PatternObject["predicates"].get_array()) {
      ThePattern.NamedPredicates.push_back(PredIdx);
    }
    Patterns.push_back(ThePattern);
  }
  return Patterns;
}

std::vector<Matcher> getMatchers(ondemand::document &TableJSON) {
  std::vector<Matcher> Matchers;
  for (ondemand::object MatcherObject : TableJSON["matchers"]) {
    Matcher TheMatcher;
    TheMatcher.Begin = MatcherObject["index"];
    TheMatcher.Kind = (Matcher::KindTy)(int)MatcherObject["kind"].get_int64();
    size_t Size = MatcherObject["size"];
    TheMatcher.End = TheMatcher.Begin + Size - 1;
    if (TheMatcher.hasPattern()) {
      TheMatcher.PIdx = MatcherObject["pattern"];
    } else if (TheMatcher.hasPatPred()) {
      TheMatcher.PIdx = MatcherObject["predicate"];
    } else if (TheMatcher.isCase()) {
      TheMatcher.CaseName = MatcherObject["case"].get_string().value();
    }
    Matchers.push_back(TheMatcher);
  }
  return Matchers;
}

MatcherTree::MatcherTree(const std::string &Filename, bool NameCaseSensitive,
                         size_t Verbosity) {
  padded_string TablePaddedStr = padded_string::load(Filename);
  ondemand::parser Parser;
  ondemand::document TableJSON = Parser.iterate(TablePaddedStr);

  Matchers = getMatchers(TableJSON);
  Patterns = getPatterns(TableJSON);
  std::sort(Matchers.begin(), Matchers.end());
  Matchers[0].End++; // final null terminator

  Verbosity = Verbosity;
  Predicates.Verbosity = Verbosity;
  Predicates.IsCaseSensitive = NameCaseSensitive;
  if (Verbosity > 1)
    errs() << "NOTE: Adding named predicates.\n";
  Predicates.addNamedPredicates(getStringArray(TableJSON, "predicates"));
  if (Verbosity > 1)
    errs() << "NOTE: Adding pattern predicates.\n";
  Predicates.addPatternPredicates(getStringArray(TableJSON, "pat_predicates"));
  MatcherTableSize = TableJSON["table_size"];
}

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
  if (Matchers.empty())
    return std::tuple(0, std::vector<bool>(), PatPredBlameList());
  std::vector<bool> ShadowMap(Matchers[0].size());
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
  if (Matchers[I].isLeaf()) {
    // We only care about leaves with a pattern or pattern predicate index
    if (Matchers[I].hasPattern() && Predicates.Verbosity &&
        !Predicates.CustomizedPatternPredicates) {
      const Pattern &Pat = Patterns[Matchers[I].PIdx];
      // Verify that all named predicates are satisfied.
      for (size_t Pred : Pat.NamedPredicates) {
        if (!Predicates.name(Pred)->satisfied()) {
          // In certain cases, some matchers have the same TableGen pattern but
          // different predicates, and the one with a different predicate is not
          // captured. They don't really cause an issue for the calculation, so
          // we just emit an error message and move on.
          errs()
              << "ERROR: Failed named predicate check " << Pred << " at "
              << Matchers[I].Begin << ".\n"
              << "ERROR: Reached leaf when named predicate is unsatisfied!\n";
        }
      }
    } else if (Matchers[I].hasPatPred()) {
      if (!Predicates.pat(Matchers[I].PIdx)->satisfied()) {
        I++;
        return true;
      } else if (Predicates.Verbosity > 2) {
        dbgs() << "DEBUG: Passed pattern predicate check " << Matchers[I].PIdx
               << " at " << Matchers[I].Begin << ".\n";
      }
    }
    I++;
    return false;
  }

  // We have a switch, scope, or a child (i.e. switch or scope case)
  size_t PI = I; // parent index
  I++;
  bool Failed = false;
  for (; I < Matchers.size() && Matchers[PI].contains(Matchers[I]);) {
    if (!Failed) {
      Failed = getUpperBound(I, UpperBound, ShadowMap, BlameMap);
    } else { // Previous pattern predicate check failed
      size_t Loss = Matchers[PI].End - Matchers[I].Begin + 1;
      BlameMap[Matchers[I - 1].PIdx] += Loss;
      UpperBound -= Loss;

      if (Predicates.Verbosity > 2)
        errs() << "DEBUG: Failed pattern predicate check "
               << Matchers[I - 1].PIdx << " at " << Matchers[I - 1].Begin
               << " with parent kind " << Matchers[PI].getKindAsString()
               << " (-" << Loss << ").\n";

      std::fill(ShadowMap.begin() + Matchers[I].Begin,
                ShadowMap.begin() + Matchers[PI].End + 1, true);

      // Fast-forward out of the parent
      while (I < Matchers.size() && Matchers[PI].contains(Matchers[I]))
        I++;
      return false;
    }
  }
  return false;
}

std::tuple<MatcherBlameList, PatPredBlameList>
MatcherTree::analyzeMap(const std::vector<bool> &ShadowMap) {
  if (Matchers.empty())
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
  if (Matchers[I].isLeaf()) {
    if (Predicates.Verbosity > 2 && !ShadowMap[Matchers[I].Begin])
      errs() << "DEBUG: Covered matcher " << Matchers[I].getKindAsString()
             << " at " << Matchers[I].Begin << ".\n";
    return ShadowMap[Matchers[I++].Begin];
  } else if (ShadowMap[Matchers[I].Begin]) {
    // A scope or group wasn't covered,
    // either because a OPC_CheckPatternPredicate failed,
    // or because the random IR wasn't varied enough.
    I++;
    return true;
  }

  size_t PI = I; // parent index
  if (Predicates.Verbosity > 2)
    errs() << "DEBUG: Entering " << Matchers[I].getKindAsString() << " at "
           << Matchers[I].Begin << ".\n";
  I++;
  for (; I < Matchers.size() && Matchers[PI].contains(Matchers[I]);) {
    if (!analyzeMap(I, ShadowMap, MatcherBlame, PatPredBlame)) {
      continue; // Matcher is covered. Keep going.
    }
    I--; // Move to the first non-covered matcher
    if (Predicates.Verbosity > 3)
      errs() << "DEBUG: Encountered uncovered matcher "
             << Matchers[I].getKindAsString() << " at " << Matchers[I].Begin
             << " of size " << Matchers[I].size() << " with parent kind "
             << Matchers[PI].getKindAsString() << ".\n";

    size_t Loss = 0;
    const Matcher &SkipOutOf =
        !Matchers[I].isChild() ? Matchers[PI] : Matchers[I];
    bool UncoveredIsLeaf = Matchers[I].isLeaf();

    if (UncoveredIsLeaf) {
      Loss = Matchers[PI].End - Matchers[I].Begin + 1;
    } else {
      Loss = Matchers[I].size();
    }
    if (!Matchers[I].isChild()) {
      if (Predicates.Verbosity > 3)
        errs() << "DEBUG: Blaming previous matcher.\n";
      I--;
    } else if (Predicates.Verbosity > 3) {
      errs() << "DEBUG: Blaming current matcher.\n";
    }

    if (Matchers[I].hasPatPred())
      PatPredBlame[Matchers[I].PIdx] += Loss;
    if (Predicates.Verbosity > 2) {
      if (Matchers[I].hasPatPred()) {
        errs() << "DEBUG: Blaming pattern predicate check " << Matchers[I].PIdx
               << " at " << Matchers[I].Begin << " with ancestor kind "
               << Matchers[PI].getKindAsString() << " (-" << Loss << ").\n";
      } else if (Matchers[I].isCase()) {
        errs() << "DEBUG: Blaming " << Matchers[I].getKindAsString() << " ("
               << Matchers[I].CaseName << ") at " << Matchers[I].Begin
               << " of size " << Matchers[I].size() << " (-" << Loss << ").\n";
      } else {
        errs() << "DEBUG: Blaming " << Matchers[I].getKindAsString() << " at "
               << Matchers[I].Begin << " of size " << Matchers[I].size()
               << " (-" << Loss << ").\n";
      }
    }
    MatcherBlame[Matchers[I].Kind] += Loss;

    if (Predicates.Verbosity > 3)
      errs() << "DEBUG: Skipping out of matcher " << SkipOutOf.getKindAsString()
             << " at " << SkipOutOf.Begin << " to after " << SkipOutOf.End
             << ".\n";

    while (I < Matchers.size() && SkipOutOf.contains(Matchers[I].Begin)) {
      I++;
    }
    if (Predicates.Verbosity > 3 && I < Matchers.size())
      errs() << "DEBUG: Skipped to node at " << Matchers[I].Begin << ".\n";

    if (UncoveredIsLeaf) {
      if (Predicates.Verbosity > 2)
        errs() << "DEBUG: Leaving " << Matchers[PI].getKindAsString()
               << " from " << Matchers[PI].End << ".\n";
      return false;
    }
  }
  if (!Matchers[PI].isLeaf() && !Matchers[PI].isChild() &&
      ShadowMap[Matchers[PI].End]) {
    // Instruction selected early. The last 0 terminator was not reached.
    MatcherBlame[Matchers[PI].Kind]++;
  }
  if (Predicates.Verbosity > 2)
    errs() << "DEBUG: Leaving " << Matchers[PI].getKindAsString() << " from "
           << Matchers[PI].End << ".\n";
  return false;
}
