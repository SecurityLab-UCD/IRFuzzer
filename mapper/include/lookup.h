#pragma once
#ifndef LOOKUP_H_
#define LOOKUP_H_

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/JSON.h"
#include <cstddef>
#include <string>

struct Matcher {
  size_t index;
  size_t size;
  int kind; // Matcher::KindTy enum
  size_t pattern;

  inline Matcher() : index(0), size(0), kind(0), pattern(0) {}
  bool operator<(const Matcher &other) const;
  inline bool hasPattern() const {
    // NOTE: These values must match the enum values of Matcher::CompleteMatch
    // and Matcher::MorphNodeTo. DAG ISel Matcher::KindTy is defined in
    // llvm/utils/TableGen which we don't have access to.
    return kind == 35 || kind == 36;
  }
};

struct Pattern {
  llvm::StringRef path;
  llvm::StringRef pattern;
  llvm::SmallVector<size_t, 3> predicates;
};

llvm::Expected<llvm::json::Value>
parseLookupTable(const std::string &LookupFilename);

std::vector<llvm::StringRef>
getPredicates(const llvm::json::Object &LookupTable);

std::vector<Pattern> getPatterns(const llvm::json::Object &LookupTable);

std::vector<Matcher> getMatchers(const llvm::json::Object &LookupTable);
#endif // LOOKUP_H_