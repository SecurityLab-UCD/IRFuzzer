#pragma once
#ifndef LOOKUP_H_
#define LOOKUP_H_

#include "predicate.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/JSON.h"
#include <string>

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

    Child, // Custom kind representing a switch / scope child

    // Highest enum value; watch out when adding more.
    HighestKind = MorphNodeTo
  };

  size_t Idx;
  size_t Size;
  KindTy Kind;
  std::optional<size_t> PatternIdx;
  std::optional<size_t> PatPredIdx;

  Matcher() : Idx(), Size(), Kind() {}
  bool operator<(const Matcher &M) const;
  bool hasPattern() const {
    return Kind == CompleteMatch || Kind == MorphNodeTo;
  }
};

struct Pattern {
  std::string IncludePath;
  std::string PatternSrc;
  llvm::SmallVector<size_t, 3> NamedPredicates;

  // Index of this pattern in lookup table (for debugging purposes)
  size_t Index;
};

struct LookupTable {
  std::vector<Pattern> Patterns;
  // Always sorted by index
  std::vector<Matcher> Matchers;
  size_t MatcherTableSize;
  PredicateKeeper PK;

  static LookupTable fromFile(const std::string &Filename,
                              bool NameCaseSensitive = false);
};

#endif // LOOKUP_H_