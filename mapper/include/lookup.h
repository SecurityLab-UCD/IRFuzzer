#pragma once
#ifndef LOOKUP_H_
#define LOOKUP_H_

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/JSON.h"
#include <string>

struct Matcher {
  size_t Idx;
  size_t Size;
  int Kind; // Matcher::KindTy enum
  size_t PatternIdx;

  Matcher() : Idx(0), Size(0), Kind(0), PatternIdx(0) {}
  bool operator<(const Matcher &M) const;
  bool hasPattern() const {
    // NOTE: These values must match the enum values of Matcher::CompleteMatch
    // and Matcher::MorphNodeTo. DAG ISel Matcher::KindTy is defined in
    // llvm/utils/TableGen which we don't have access to.
    return Kind == 35 || Kind == 36;
  }
};

struct Pattern {
  llvm::StringRef IncludePath;
  llvm::StringRef PatternSrc;
  llvm::SmallVector<size_t, 3> Predicates;
};

llvm::Expected<llvm::json::Value>
parseLookupTable(const std::string &LookupFilename);

std::vector<llvm::StringRef>
getPredicates(const llvm::json::Object &LookupTable);

std::vector<Pattern> getPatterns(const llvm::json::Object &LookupTable);

std::vector<Matcher> getMatchers(const llvm::json::Object &LookupTable);
#endif // LOOKUP_H_