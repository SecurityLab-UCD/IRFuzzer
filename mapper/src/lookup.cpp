#include "lookup.h"

#include "simdjson.h"
#include <fstream>

using namespace llvm;
using namespace simdjson;

bool Matcher::operator<(const Matcher &M) const {
  return Begin == M.Begin ? End > M.End : Begin < M.Begin;
}

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
    }
    Matchers.push_back(TheMatcher);
  }
  return Matchers;
}

LookupTable LookupTable::fromFile(const std::string &Filename,
                                  bool NameCaseSensitive, size_t Verbosity) {
  padded_string TablePaddedStr = padded_string::load(Filename);
  ondemand::parser Parser;
  ondemand::document TableJSON = Parser.iterate(TablePaddedStr);
  LookupTable Table;

  Table.Matchers = getMatchers(TableJSON);
  Table.Patterns = getPatterns(TableJSON);
  std::sort(Table.Matchers.begin(), Table.Matchers.end());
  Table.Matchers[0].End++;

  Table.PK.Verbosity = Verbosity;
  Table.PK.IsCaseSensitive = NameCaseSensitive;
  if (Table.PK.Verbosity > 1)
    errs() << "NOTE: Adding named predicates.\n";
  Table.PK.addNamedPredicates(getStringArray(TableJSON, "predicates"));
  if (Table.PK.Verbosity > 1)
    errs() << "NOTE: Adding pattern predicates.\n";
  Table.PK.addPatternPredicates(getStringArray(TableJSON, "pat_predicates"));
  Table.MatcherTableSize = TableJSON["table_size"];

  return Table;
}