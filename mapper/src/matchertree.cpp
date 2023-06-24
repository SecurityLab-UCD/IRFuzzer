#include "matchertree.h"
#include "simdjson.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <cmath>
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
  case Subscope:
  case SwitchOpcode:
  case SwitchType:
  case SwitchOpcodeCase:
  case SwitchTypeCase:
    return false;
  }
}

bool Matcher::hasLeafSibling() const { return !isCase() && Kind != Subscope; }

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
    ENUM_TO_STR(Subscope)
    ENUM_TO_STR(SwitchTypeCase)
    ENUM_TO_STR(SwitchOpcodeCase)
  default:
    return "Unknown";
  }
}
#undef ENUM_TO_STR

Blamee::Blamee(size_t MatcherIdx, size_t Depth)
    : MatcherIdx(MatcherIdx), Loss(1), Depth(Depth), isEarlyExit(true) {}

std::string Matcher::getKindAsString() const { return getKindAsString(Kind); }

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
    if (ThePattern.NamedPredicates.size()) {
      try {
        ThePattern.PatPredIdx = PatternObject["pat_predicate"];
      } catch (simdjson_error &) {
        // The named predicate is a TruePredicate.
        ThePattern.NamedPredicates.clear();
      }
    } else {
      ThePattern.PatPredIdx = std::numeric_limits<size_t>::max();
    }
    // explicitly cast to prevent using operator bool()
    ThePattern.Complexity = (int64_t)PatternObject["complexity"];
    ThePattern.IncludePath = PatternObject["path"].get_string().value();
    StringRef SrcDst = PatternObject["pattern"].get_string().value();
    std::tie(ThePattern.Src, ThePattern.Dst) = SrcDst.split(" -> ");
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
      TheMatcher.PatternIdx = MatcherObject["pattern"];
    } else if (TheMatcher.hasPatPred()) {
      TheMatcher.PatPredIdx = MatcherObject["predicate"];
    } else if (TheMatcher.isCase()) {
      TheMatcher.CaseName = MatcherObject["case"].get_string().value();
    }
    Matchers.push_back(TheMatcher);
  }
  return Matchers;
}

MatcherTree::MatcherTree(const std::string &Filename, bool NameCaseSensitive,
                         size_t Verbosity)
    : Verbosity(Verbosity) {
  padded_string TablePaddedStr = padded_string::load(Filename);
  ondemand::parser Parser;
  ondemand::document TableJSON = Parser.iterate(TablePaddedStr);

  Matchers = getMatchers(TableJSON);
  Patterns = getPatterns(TableJSON);
  std::sort(Matchers.begin(), Matchers.end());
  if (Matchers.size())
    Matchers[0].End++; // final null terminator

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
std::vector<std::pair<A, B>> toSortedVecByValue(const M<A, B, Args...> &Src) {
  std::vector<std::pair<A, B>> Dst(Src.size());
  std::copy(Src.begin(), Src.end(), Dst.begin());
  std::sort(Dst.begin(), Dst.end(),
            [](const auto &L, const auto &R) { return L.second > R.second; });
  return Dst;
}

template <typename A, typename B, template <class, class, class...> class M,
          class... Args>
std::vector<std::pair<A, B>> toSortedVecByKey(const M<A, B, Args...> &Src) {
  std::vector<std::pair<A, B>> Dst(Src.size());
  std::copy(Src.begin(), Src.end(), Dst.begin());
  std::sort(Dst.begin(), Dst.end(),
            [](const auto &L, const auto &R) { return L.first < R.first; });
  return Dst;
}

void MatcherTree::analyzeUpperBound() {
  I = 0;
  ShadowMap.clear();
  // Start by assuming that all indices could be covered
  ShadowMap.resize(MatcherTableSize, false);
  if (Matchers.size()) {
    getUpperBound();
    analyzeMap(ShadowMap);
  }
}

/// @param Kind matcher kind
/// @return true if the given matcher kind could fail, which means that the
/// current immediate subscope may not always succeed & lead to early match.
bool affectsEarlyMatch(Matcher::KindTy Kind) {
  switch (Kind) {
  default:
    return false;
  case Matcher::CheckSame:
  case Matcher::CheckChildSame:
  case Matcher::CheckPredicate:
  case Matcher::CheckType:
  case Matcher::CheckChildType:
  case Matcher::CheckInteger:
  case Matcher::CheckChildInteger:
  case Matcher::CheckCondCode:
  case Matcher::CheckChild2CondCode:
  case Matcher::CheckValueType:
  case Matcher::CheckAndImm:
  case Matcher::CheckOrImm:
  case Matcher::CheckImmAllOnesV:
  case Matcher::CheckImmAllZerosV:
  case Matcher::CheckFoldableChainNode:
    return true;
  }
}

/// @brief Visit a matcher tree node and calculate coverage upper bound
/// @return whether this leaf failed (e.g. by a pattern predicate check)
bool MatcherTree::getUpperBound() {
  if (Matchers[I].isLeaf()) {
    if (Matchers[I].hasPattern()) {
      MatchedPattern = true;
    }
    // We only care about leaves with a pattern or pattern predicate index
    if (Matchers[I].hasPattern() && Predicates.Verbosity &&
        !Predicates.CustomizedPatternPredicates) {
      const Pattern &Pat = Patterns[Matchers[I].PatternIdx];
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
    } else if (Matchers[I].hasPatPred() &&
               !Predicates.pat(Matchers[I].PatPredIdx)->satisfied()) {
      I++;
      return true;
    }
    I++;
    return false;
  }

  // We have a switch/switch case, or scope/subscope
  size_t PI = I; // parent index
  I++;
  bool Failed = false;
  bool AlwaysEarlyMatch = true;
  for (; I < Matchers.size() && Matchers[PI].contains(Matchers[I]);) {
    if (!Failed) {
      size_t CI = I;
      Failed = getUpperBound();
      if (Matchers[PI].Kind == Matcher::Subscope) {
        AlwaysEarlyMatch =
            AlwaysEarlyMatch && !affectsEarlyMatch(Matchers[CI].Kind);
      }
      if (MatchedPattern && Matchers[PI].Kind == Matcher::Subscope) {
        MatchedPattern = false;
        if (!AlwaysEarlyMatch) {
          AlwaysEarlyMatch = true;
          continue;
        }
        if (Verbosity > 3) {
          errs() << "DEBUG: Got possible early match at " << Matchers[CI].Begin
                 << '\n';
        }
        // Match completed. Rest of Scope should be uncovered.
        return true;
      }
    } else {
      // Either a pattern predicate check predecessor failed, or instruction was
      // already selected in a subscope predecessor. Mark the rest of parent as
      // uncovered.
      std::fill(ShadowMap.begin() + Matchers[I].Begin,
                ShadowMap.begin() + Matchers[PI].End + 1, true);

      // Fast-forward out of the parent
      while (I < Matchers.size() && Matchers[PI].contains(Matchers[I]))
        I++;
      return false;
    }
  }
  MatchedPattern = false;
  return false;
}

void MatcherTree::analyzeMap(const std::vector<bool> &Map) {
  if (Matchers.empty())
    return;
  I = 0;
  CurrentDepth = 0;
  ShadowMap = Map;
  BlameList.clear();
  if (!std::count(Map.begin(), Map.end(), false)) {
    // Handle blank shadow map by blaming outermost matcher
    Blamee TheBlamee;
    TheBlamee.Loss = Map.size();
    for (size_t I = 0; I < Patterns.size(); I++)
      TheBlamee.Blamers.insert(I);
    BlameList.push_back(TheBlamee);
  } else {
    analyzeMap();
  }
}

std::vector<std::pair<size_t, size_t>>
MatcherTree::blamePatternPredicates() const {
  std::unordered_map<size_t, size_t> BlameMap;
  for (const Blamee &TheBlamee : BlameList) {
    const Matcher &M = Matchers[TheBlamee.MatcherIdx];
    if (!M.hasPatPred())
      continue;
    BlameMap[M.PatPredIdx] += TheBlamee.Loss;
  }
  return toSortedVecByValue(BlameMap);
}

std::vector<std::pair<Matcher::KindTy, size_t>>
MatcherTree::blameMatcherKinds() const {
  std::unordered_map<Matcher::KindTy, size_t> BlameMap;
  for (const Blamee &TheBlamee : BlameList) {
    const Matcher &M = Matchers[TheBlamee.MatcherIdx];
    BlameMap[M.Kind] += TheBlamee.Loss;
  }
  return toSortedVecByValue(BlameMap);
}

std::vector<std::pair<size_t, size_t>>
MatcherTree::blameDepth(std::optional<Matcher::KindTy> Kind) const {
  std::unordered_map<size_t, size_t> DBM;
  for (const Blamee &TheBlamee : BlameList) {
    if (Kind.has_value() && Matchers[TheBlamee.MatcherIdx].Kind != Kind.value())
      continue;
    DBM[TheBlamee.Depth] += TheBlamee.Loss;
  }
  return toSortedVecByKey(DBM);
}

std::vector<std::tuple<size_t, size_t, size_t, std::string, std::string>>
MatcherTree::blamePatterns(bool UseLossPerPattern) const {
  std::vector<std::tuple<size_t, size_t, size_t, std::string, std::string>>
      FailedPatterns;
  for (const Blamee &TheBlamee : BlameList) {
    if (TheBlamee.Blamers.size() == 0) {
      continue; // Ignore uncovered null terminator
    }
    size_t Loss = TheBlamee.Loss;
    if (UseLossPerPattern) {
      Loss /= TheBlamee.Blamers.size();
    }
    size_t BlameeMTIdx = Matchers[TheBlamee.MatcherIdx].Begin;
    std::string BlameeKind = Matchers[TheBlamee.MatcherIdx].getKindAsString();
    for (size_t Blamer : TheBlamee.Blamers) {
      const Pattern &P = Patterns[Blamer];
      FailedPatterns.push_back(std::tuple(Loss, BlameeMTIdx, TheBlamee.Depth,
                                          BlameeKind, P.Src + " -> " + P.Dst));
    }
  }
  std::sort(FailedPatterns.begin(), FailedPatterns.end(),
            [](const auto &L, const auto &R) {
              return std::get<0>(L) > std::get<0>(R);
            });
  return FailedPatterns;
}

std::vector<std::string> MatcherTree::blamePossiblePatterns() const {
  // Use set since there may be patterns that only differ in pattern
  // predicates but otherwise do not have different source pattern strings.
  std::set<std::string> PossiblePatterns;
  for (const Blamee &TheBlamee : BlameList) {
    if (TheBlamee.Blamers.size() == 0)
      continue;
    if (Matchers[TheBlamee.MatcherIdx].Kind == Matcher::CheckPatternPredicate)
      continue;
    for (size_t PatIdx : TheBlamee.Blamers) {
      PossiblePatterns.insert(Patterns[PatIdx].Src);
    }
  }
  return std::vector<std::string>(PossiblePatterns.begin(),
                                  PossiblePatterns.end());
}

std::vector<Intrinsic::ID> MatcherTree::blameTargetIntrinsic() const {
  std::set<Intrinsic::ID> TargetIIDs;
  for (const Blamee &TheBlamee : BlameList) {
    if (TheBlamee.Blamers.size() == 0)
      continue;
    for (size_t PatIdx : TheBlamee.Blamers) {
      std::smatch SMatch;
      // For now, only match nodes with no nesting (top-level INTRINSIC_* switch
      // opcode cases)
      static std::regex MatchIntrinsicID{"^\\(intrinsic_.*? (\\d+):"};
      if (!std::regex_search(Patterns[PatIdx].Src, SMatch, MatchIntrinsicID))
        continue; // Pattern is not an intrinsic function
      Intrinsic::ID IID = std::stoul(SMatch[1]);

      // A few of them are not target intrinsic functions
      if (Function::isTargetIntrinsic(IID))
        TargetIIDs.insert(IID);
    }
  }
  return std::vector<Intrinsic::ID>(TargetIIDs.begin(), TargetIIDs.end());
}

/// @brief Generate blame list for the current ShadowMap
/// @return whether the current matcher failed / was not covered
bool MatcherTree::analyzeMap() {
  if (Matchers[I].isLeaf()) {
    return ShadowMap[Matchers[I++].Begin];
  } else if (ShadowMap[Matchers[I].Begin]) {
    // A non-leaf matcher wasn't covered,
    // either because a OPC_CheckPatternPredicate failed,
    // or because the random IR wasn't varied enough.
    I++;
    return true;
  }

  size_t PI = I; // parent index
  I++;
  if (Matchers[PI].hasLeafSibling())
    CurrentDepth++;
  for (; I < Matchers.size() && Matchers[PI].contains(Matchers[I]);) {
    if (!analyzeMap()) {
      continue; // Matcher is covered. Keep going.
    }
    I--; // Move to the first uncovered matcher

    Blamee TheBlamee;
    // TODO: check if we are storing the right depth
    TheBlamee.Depth = CurrentDepth;

    // If the uncovered matcher may have leaf siblings (i.e. not cases or scope
    // groups), then we know the rest of the parent is uncovered and we skip out
    // of it.
    const Matcher &SkipOutOf =
        Matchers[I].hasLeafSibling() ? Matchers[PI] : Matchers[I];
    bool UncoveredIsLeaf = Matchers[I].isLeaf();

    if (UncoveredIsLeaf) {
      TheBlamee.Loss = Matchers[PI].End - Matchers[I].Begin + 1;
    } else {
      TheBlamee.Loss = Matchers[I].size();
    }

    if (Matchers[I].hasLeafSibling()) {
      // If the uncovered matcher may have leaf siblings, then we know that it
      // must have been failed by some kind of check (previous sibling) or case
      // condition (parent). In both cases, we blame I - 1.
      I--;
    } else {
      // Case or scope group not reached since instruction was already selected.
      TheBlamee.isEarlyExit = true;
    }
    TheBlamee.MatcherIdx = I;
    I++; // Move to first descendant matcher (if any)

    // Record all patterns not reached
    for (; I < Matchers.size() && SkipOutOf.contains(Matchers[I].Begin); I++) {
      const Matcher &M = Matchers[I];
      if (!M.hasPattern())
        continue;
      TheBlamee.Blamers.insert(M.PatternIdx);
      if (Verbosity > 3) {
        errs() << "DEBUG:     Blamer: Pattern " << M.PatternIdx << " at "
               << M.Begin << " with complexity "
               << Patterns[M.PatternIdx].Complexity << '\n';
      }
    }
    if (TheBlamee.Blamers.size() == 0) {
      errs() << "ERROR:     No blamers found for blamee.\n";
    }

    BlameList.push_back(TheBlamee);

    if (Verbosity > 3) {
      errs() << "DEBUG: ";
      const Matcher &M = Matchers[TheBlamee.MatcherIdx];
      errs() << "Blaming " << M.getKindAsString();
      if (M.isCase()) {
        errs() << " (" << M.CaseName << ')';
      } else if (M.hasPatPred()) {
        errs() << " (" << M.PatPredIdx << ")";
      }
      errs() << " at " << M.Begin << " (depth " << CurrentDepth << ") of size "
             << M.size() << " (-" << TheBlamee.Loss << ")\n";
    }

    if (UncoveredIsLeaf) {
      if (Matchers[PI].hasLeafSibling())
        CurrentDepth--;
      return false;
    }
  }
  if (Matchers[PI].hasLeafSibling())
    CurrentDepth--;
  if (Matchers[PI].hasLeafSibling() && ShadowMap[Matchers[PI].End]) {
    // Instruction selected early. The last 0 terminator was not reached.
    BlameList.push_back(Blamee(PI, CurrentDepth));
  }
  return false;
}
