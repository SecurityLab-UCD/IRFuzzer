#include "lookup.h"

#include <fstream>

using namespace llvm;

bool Matcher::operator<(const Matcher &other) const {
  if (index < other.index)
    return true;
  return index == other.index && size > other.size;
}

// NOTE: exits program if error encountered
Expected<json::Value> parseLookupTable(const std::string &LookupFilename) {
  std::ifstream LookupIfs(LookupFilename);
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
  Expected<json::Value> ParseResult = json::parse(LookupTableStr);
  if (!ParseResult) {
    errs() << "Error parsing lookup table.\n";
    exit(1);
  }
  return ParseResult;
}

std::vector<StringRef> getPredicates(const json::Object &LookupTable) {
  std::vector<StringRef> Predicates;
  for (const auto &Predicate : *LookupTable.getArray("predicates")) {
    Predicates.push_back(Predicate.getAsString().value());
  }
  return Predicates;
}

std::vector<Pattern> getPatterns(const json::Object &LookupTable) {
  std::vector<Pattern> Patterns;
  for (const json::Value &PatternObject : *LookupTable.getArray("patterns")) {
    Pattern ThePattern;
    ThePattern.path = (*PatternObject.getAsObject()).getString("path").value();
    ThePattern.pattern =
        (*PatternObject.getAsObject()).getString("pattern").value();
    for (const json::Value &PredIdx :
         *(*PatternObject.getAsObject()).getArray("predicates")) {
      ThePattern.predicates.push_back(PredIdx.getAsInteger().value());
    }
    Patterns.push_back(ThePattern);
  }
  return Patterns;
}

std::vector<Matcher> getMatchers(const json::Object &LookupTable) {
  std::vector<Matcher> Matchers;
  for (const json::Value &MatcherObject : *LookupTable.getArray("matchers")) {
    Matcher TheMatcher;
    TheMatcher.index = MatcherObject.getAsObject()->getInteger("index").value();
    TheMatcher.size = MatcherObject.getAsObject()->getInteger("size").value();
    TheMatcher.kind = MatcherObject.getAsObject()->getInteger("kind").value();

    if (TheMatcher.hasPattern())
      TheMatcher.pattern =
          MatcherObject.getAsObject()->getInteger("pattern").value();
    Matchers.push_back(TheMatcher);
  }
  return Matchers;
}