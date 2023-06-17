#pragma once
#ifndef MATCHER_TREE_H_
#define MATCHER_TREE_H_
#include "predicate.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
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

  size_t Begin = 0;
  size_t End = 0;
  KindTy Kind = Scope;
  // To save space, this may either be a pattern or a pattern predicate index
  size_t PIdx = 0;
  std::string CaseName;

  bool operator<(const Matcher &M) const;
  bool operator==(const Matcher &N) const;
  bool operator!=(const Matcher &N) const;

  bool contains(size_t i) const;
  bool contains(const Matcher &N) const;
  bool hasPattern() const;
  bool hasPatPred() const;
  bool isLeaf() const;
  bool hasLeafSibling() const;
  bool isCase() const;
  size_t size() const;

  static std::string getKindAsString(KindTy Kind);
  std::string getKindAsString() const;
};

typedef std::vector<std::pair<size_t, size_t>> PatPredBlameList;
typedef std::vector<std::pair<Matcher::KindTy, size_t>> MatcherKindBlameList;

struct Pattern {
  std::string IncludePath;
  std::string Src;
  std::string Dst;
  llvm::SmallVector<size_t, 3> NamedPredicates;
  size_t Index; // Index in the lookup table array
  size_t Complexity;
};

struct Blamee {
  size_t MatcherIdx = 0;              // index in the Matchers vector
  size_t Loss = 0;                    // coverage loss
  std::unordered_set<size_t> Blamers; // index to patterns
  size_t Depth = 1;                   // how nested the blamed matcher is
  bool isEarlyExit = false;           // true = the blamee itself is uncovered

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
  PredicateKeeper Predicates;
  // Always sorted by matcher table index then size
  std::vector<Matcher> Matchers;
  std::vector<Blamee> BlameList;
  std::vector<bool> ShadowMap;
  size_t Verbosity = 0;

  // Read from pattern lookup table JSON
  MatcherTree(const std::string &Filename, bool NameCaseSensitive,
              size_t Verbosity);
  MatcherTree(const MatcherTree &) = delete;
  MatcherTree(MatcherTree &&) = default;

  /// @brief Determine coverage upper bound shadow map and analyze it
  /// @return reference to upper bound shadow map
  const std::vector<bool> &analyzeUpperBound();

  /// @brief Analyze coverage loss in shadow map and populate BlameList
  /// @param Map map to be analyzed
  void analyzeMap(const std::vector<bool> &Map);

  PatPredBlameList blamePatternPredicates() const;
  MatcherKindBlameList blameMatcherKinds() const;
  std::vector<std::pair<size_t, size_t>> blameDepth() const;
  std::vector<std::pair<size_t, size_t>> blameSOCAtDepth() const;
  // returns [(blamer loss, blamee MT index, blamee kind, src -> dst)...]
  std::vector<std::tuple<size_t, size_t, size_t, std::string, std::string>>
  blamePatterns(bool UseLossPerPattern) const;

  // returns [pattern source...]
  // returns only pattern source strings that are possible (i.e. not failed
  // by pattern predicate check)
  std::set<std::string> blamePossiblePatterns() const;

private:
  bool getUpperBound();
  bool analyzeMap();
};
#endif // MATCHER_TREE_H_
