#include "predicate.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"
#include <regex>

using namespace llvm;

void PredicateKeeper::addNamedPredicates(
    const std::vector<std::string> &Records) {

  // Load named predicates
  // We need to find literal predicates first, and then
  // parse composite predicates

  // Composite predicates to be parsed after literals
  std::map<std::string, std::string> NamedPredsToParse;

  std::regex MatchCondString(R"_(string CondString = "(.+?)";\n)_");
  std::regex MatchNonLiteralPredicate("!|\\||&");

  // Find literals
  for (const std::string &Record : Records) {
    StringRef NameRef = StringRef(Record).split(" ").first;
    std::string Name = IsCaseSensitive ? NameRef.str() : NameRef.lower();
    std::smatch Match;

    if (!std::regex_search(Record, Match, MatchCondString)) {
      errs() << "Failed to extract condition for predicate " << Name << "\n";
#ifndef NDEBUG
      errs() << Record << "\n";
#endif
      exit(1);
    }

    std::string CondString = Match.str(1);
    if (CondString.empty()) {
      errs() << "Got empty condition for predicate " << Name << "\n";
#ifndef NDEBUG
      errs() << Record << "\n";
#endif
      exit(1);
    }
    Predicate *P = nullptr;
    if (std::regex_search(CondString, MatchNonLiteralPredicate)) {
      NamedPredsToParse.insert(std::make_pair(Name, CondString));
      // Parse the composite predicate and update pointer later
    } else {
      P = CondString == "true" ? True : new LiteralPredicate(false);
      if (P != True)
        AllPredicates.push_back(P);
      LiteralExpressions.insert(std::make_pair(CondString, Name));
    }
    NamedPredLookup[Name] = NamedPredicates.size();
    NamedPredicates.push_back(P);
  }

  // Parse composite predicate expressions
  for (const auto &[Name, CondString] : NamedPredsToParse) {
    Predicate *P = parsePredicate(CondString);
    if (!P) {
      errs() << "Failed to parse condition for predicate " << Name << ".\n";
#ifndef NDEBUG
      errs() << "CondString = " << CondString << "\n";
#endif
      exit(1);
    }
    NamedPredicates[NamedPredLookup[Name]] = P;
  }
}

void PredicateKeeper::addPatternPredicates(
    const std::vector<std::string> &Expressions) {
  for (const std::string &P : Expressions) {
    PatternPredicates.push_back(parsePredicate(P));
  }
}

// Poor man's parser for simple C++ expressions found in predicates
// We don't need tokenization because TableGen's formatting is neat.
Predicate *PredicateKeeper::parsePredicate(const std::string &CondString) {
  if (CondString.empty()) {
    notFound("<expr>", CondString, 0);
  }
  size_t CurIndex = 0;
  return parseExpr(CondString, CurIndex);
}

Predicate *PredicateKeeper::parseExpr(const std::string &CondString,
                                      size_t &CurIndex) {
  return parseOr(CondString, CurIndex);
}

Predicate *PredicateKeeper::parseGroup(const std::string &CondString,
                                       size_t &CurIndex) {
  if (CondString[CurIndex] != '(') {
    notFound("(", CondString, CurIndex);
  }
  Predicate *P = parseExpr(CondString, ++CurIndex);
  if (CondString[CurIndex] != ')') {
    notFound(")", CondString, CurIndex);
  }
  CurIndex++;
  return P;
}

Predicate *PredicateKeeper::parseOr(const std::string &CondString,
                                    size_t &CurIndex) {
  std::vector<Predicate *> Children;
  Children.push_back(parseAnd(CondString, CurIndex));
  while (CondString.find(" || ", CurIndex) == CurIndex ||
         CondString.find(" ||", CurIndex) == CurIndex) {
    CurIndex += 3;
    if (CondString[CurIndex] == ' ')
      CurIndex++;
    Children.push_back(parseAnd(CondString, CurIndex));
  }
  if (Children.size() == 1) {
    return Children[0];
  }
  Predicate *P = new OrPredicate(std::move(Children));
  AllPredicates.push_back(P);
  return P;
}

Predicate *PredicateKeeper::parseAnd(const std::string &CondString,
                                     size_t &CurIndex) {
  std::vector<Predicate *> Children;
  Children.push_back(parseNot(CondString, CurIndex));
  while (CondString.find(" && ", CurIndex) == CurIndex ||
         CondString.find(" &&", CurIndex) == CurIndex) {
    CurIndex += 3;
    if (CondString[CurIndex] == ' ')
      CurIndex++;
    Children.push_back(parseNot(CondString, CurIndex));
  }
  if (Children.size() == 1) {
    return Children[0];
  }
  Predicate *P = new AndPredicate(std::move(Children));
  AllPredicates.push_back(P);
  return P;
}

Predicate *PredicateKeeper::parseNot(const std::string &CondString,
                                     size_t &CurIndex) {
  if (CondString[CurIndex] == '!') {
    Predicate *PChild = parseLiteral(CondString, ++CurIndex);
    Predicate *P = new NotPredicate(PChild);
    AllPredicates.push_back(P);
    return P;
  }
  return parseLiteral(CondString, CurIndex);
}

Predicate *PredicateKeeper::parseLiteral(const std::string &CondString,
                                         size_t &CurIndex) {
  if (CondString[CurIndex] == '(') {
    return parseGroup(CondString, CurIndex);
  }

  std::smatch SMatch;
  // Probably pretty slow?
  bool Matched = std::regex_search(CondString.begin() + CurIndex,
                                   CondString.end(), SMatch, MatchLiteral);

  if (!Matched) {
    notFound("<literal>", CondString, CurIndex);
  }

  std::string Expr = SMatch.str(0);
  CurIndex += Expr.size();
  if (LiteralExpressions.count(Expr) == 0) {
#ifndef NDEBUG
    errs() << "Found literal with unknown value: " << Expr
           << ". Defaulting to false.\n";
#endif
    LiteralExpressions[Expr] = "FalsePredicate";
  }
  return name(LiteralExpressions.at(Expr));
}

// Helper function to report parsing errors and exit program
[[noreturn]] void PredicateKeeper::notFound(const std::string &Str,
                                            const std::string &CondString,
                                            size_t CurIndex) {
  llvm::errs() << "Expected `" << Str << "` at char " << CurIndex + 1 << " in '"
               << CondString << "'.\n";
  exit(1);
}