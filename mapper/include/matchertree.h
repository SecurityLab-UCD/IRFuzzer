#pragma once
#ifndef MATCHER_TREE_H_
#define MATCHER_TREE_H_
#include "predicate.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Intrinsics.h"
#include <map>
#include <string>
#include <unordered_set>

struct Matcher {
  // Matcher kind borrowed from DAGISelMatcher.h
  // Keep updated with llvm's version
  enum KindTy {
    // Matcher state manipulation.
    Scope,            // Push a checking scope.
    RecordNode,       // Record the current node.
    RecordChild,      // Record a child of the current node.
    RecordMemRef,     // Record the memref in the current node.
    CaptureGlueInput, // If the current node has an input glue, save it.
    MoveChild,        // Move current node to specified child.
    MoveParent,       // Move current node to parent.

    // Predicate checking.
    CheckSame,             // Fail if not same as prev match.
    CheckChildSame,        // Fail if child not same as prev match.
    CheckPatternPredicate, //
    CheckPredicate,        // Fail if node predicate fails.
    CheckOpcode,           // Fail if not opcode.
    SwitchOpcode,          // Dispatch based on opcode.
    CheckType,             // Fail if not correct type.
    SwitchType,            // Dispatch based on type.
    CheckChildType,        // Fail if child has wrong type.
    CheckInteger,          // Fail if wrong val.
    CheckChildInteger,     // Fail if child is wrong val.
    CheckCondCode,         // Fail if not condcode.
    CheckChild2CondCode,   // Fail if child is wrong condcode.
    CheckValueType,
    CheckComplexPat,
    CheckAndImm,
    CheckOrImm,
    CheckImmAllOnesV,
    CheckImmAllZerosV,
    CheckFoldableChainNode,

    // Node creation/emisssion.
    EmitInteger,          // Create a TargetConstant
    EmitStringInteger,    // Create a TargetConstant from a string.
    EmitRegister,         // Create a register.
    EmitConvertToTarget,  // Convert a imm/fpimm to target imm/fpimm
    EmitMergeInputChains, // Merge together a chains for an input.
    EmitCopyToReg,        // Emit a copytoreg into a physreg.
    EmitNode,             // Create a DAG node
    EmitNodeXForm,        // Run a SDNodeXForm
    CompleteMatch,        // Finish a match and update the results.
    MorphNodeTo,          // Build a node, finish a match and update results.

    Subscope,         // Custom: A child of a scope
    SwitchTypeCase,   // Custom: A case of SwitchType
    SwitchOpcodeCase, // Custom: A case of SwitchOpcode

    // Highest enum value; watch out when adding more.
    HighestKind = MorphNodeTo
  };

  // [Begin, End]
  size_t Begin = 0;
  size_t End = 0;
  KindTy Kind = Scope;
  union {
    // Index to Patterns associated with a MorphNodeTo or CompleteMatch
    size_t PatternIdx;
    // Index to PredicateKeeper::PatternPredicates used by CheckPatternPredicate
    size_t PatPredIdx;
  };

  // Name of a switch case (i.e. opcode or type name)
  std::string CaseName;

  /// @param M another matcher
  /// @return whether the current matcher comes before M in
  /// matcher table
  bool operator<(const Matcher &M) const;

  /// @param N another matcher
  /// @return whether the current matcher has the same interval as N
  bool operator==(const Matcher &N) const;

  /// @param N another matcher
  /// @return !(*this == N)
  bool operator!=(const Matcher &N) const;

  /// @param i a matcher table index
  /// @return whether the matcher table index is within this matcher
  bool contains(size_t i) const;

  /// @param N a matcher
  /// @return whether the matcher fully contains N
  bool contains(const Matcher &N) const;

  /// @return whether this matcher has a PatternIdx
  bool hasPattern() const;

  /// @return whether this matcher has a PatPredIdx
  bool hasPatPred() const;

  /// @return true if this kind of matcher cannot have child matchers
  bool isLeaf() const;

  /// @return true if this kind of matcher can have siblings that are leaves
  bool hasLeafSibling() const;

  /// @return true if this matcher is a Switch{Type,Opcode}Case
  bool isCase() const;

  /// @return size of the matcher in matcher table
  size_t size() const;

  static std::string getKindAsString(KindTy Kind);
  std::string getKindAsString() const;
};

struct Pattern {
  // where the pattern was defined (a .td file name with line number)
  std::string IncludePath;
  // pattern source (uses DAG opcodes)
  std::string Src;
  // pattern destination (uses machine instruction opcodes)
  std::string Dst;
  // a list of predicates that must be satisfied for SelectionDAG to generate
  // this pattern
  llvm::SmallVector<size_t, 3> NamedPredicates;
  // Index to PredicateKeeper::PatternPredicates. Same thing as named
  // predicates, except they are concatenated into a single C++ expression. Only
  // use if !NamedPredicates.empty().
  size_t PatPredIdx;
  // The pattern's index in the lookup table array
  size_t Index;
  // Pattern's complexity as calculated by TableGen
  int Complexity;
};

struct Blamee {
  // Blamee's index in Matchers
  size_t MatcherIdx = 0;
  // Coverage loss
  size_t Loss = 0;
  // Indices to patterns that were not covered due to the blamee
  std::unordered_set<size_t> Blamers;
  // How nested the blamee is
  size_t Depth = 1;
  // If true, then the blamee itself is uncovered
  bool isEarlyExit = false;

  Blamee() = default;
  // For uncovered null terminators
  Blamee(size_t MatcherIdx, size_t Depth);
};

class MatcherTree {
  // Index to the current matcher
  size_t I = 0;
  // Nesting level of the current matcher
  size_t CurrentDepth = 0;
  // Used in getUpperBound() for handling early returns (instruction selected
  // before reaching end of scope).
  bool MatchedPattern = false;

public:
  std::vector<Pattern> Patterns;
  size_t MatcherTableSize;
  // Stores the truth value of all named predicates and pattern predicates (i.e.
  // predicate checks)
  PredicateKeeper Predicates;
  // List of matchers sorted by matcher table index and then size
  std::vector<Matcher> Matchers;
  // List of matchers that caused coverage loss
  // Populated by analyzeMap()
  std::vector<Blamee> BlameList;
  // Current shadow map under analysis
  std::vector<bool> ShadowMap;
  // For use in mapper only
  size_t Verbosity = 0;

  /// @brief Create matcher tree from pattern lookup table JSON
  /// @param Filename file path to pattern lookup table
  /// @param NameCaseSensitive whether predicate names are case-sensitive
  /// @param Verbosity verbosity; for use in mapper only
  MatcherTree(const std::string &Filename, bool NameCaseSensitive = false,
              size_t Verbosity = 0);
  MatcherTree(const MatcherTree &) = delete;
  MatcherTree(MatcherTree &&) = default;

  /// @brief Determine coverage upper bound shadow map and analyze it
  /// @note The upperbound shadow map is MatcherTree::ShadowMap. This function
  /// also calls analyzeMap().
  void analyzeUpperBound();

  /// @brief Analyze coverage loss in shadow map and populate BlameList
  /// @param Map map to be analyzed
  /// @note Run this before using the blame member functions.
  void analyzeMap(const std::vector<bool> &Map);

  /// @brief Calculate coverage loss caused by unsatisfied pattern predicates
  /// @return [(pattern predicate index, coverage loss)...]
  /// @note Run analyzeMap() beforehand
  std::vector<std::pair<size_t, size_t>> blamePatternPredicates() const;

  /// @brief Calculate coverage loss caused by different matcher kinds
  /// @return [(matcher kind, coverage loss)...]
  /// @note Run analyzeMap() beforehand
  std::vector<std::pair<Matcher::KindTy, size_t>> blameMatcherKinds() const;

  /// @brief Calculate coverage loss occurred at different nesting levels /
  /// depths
  /// @param Kind only calculate loss from blamees with a given matcher kind
  /// @return [(depth, coverage loss)...]
  /// @note Run analyzeMap() beforehand
  std::vector<std::pair<size_t, size_t>>
  blameDepth(std::optional<Matcher::KindTy> Kind = std::nullopt) const;

  /// @brief Get patterns that were not covered
  /// @param UseLossPerPattern For each blamer's coverage loss, use average loss
  /// per blamer instead of total blamee loss
  /// @return [(blamer coverage loss, blamee matcher table index, blamee depth,
  /// pattern src, pattern dst)...]
  /// @note Run analyzeMap() beforehand
  std::vector<std::tuple<size_t, size_t, size_t, std::string, std::string>>
  blamePatterns(bool UseLossPerPattern) const;

  /// @return all uncovered but possible pattern source strings (i.e. not failed
  /// by pattern predicate check)
  /// @note Run analyzeMap() beforehand
  std::vector<std::string> blamePossiblePatterns() const;

  /// @return all uncovered target intrinsic IDs in matcher table
  /// @note Run analyzeMap() beforehand
  std::vector<llvm::Intrinsic::ID> blameTargetIntrinsic() const;

private:
  bool getUpperBound();
  bool analyzeMap();
};
#endif // MATCHER_TREE_H_
