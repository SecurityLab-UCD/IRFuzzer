#include "predicate.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <regex>

using namespace llvm;

bool NotPredicate::resolve(bool NewValue) {
  return _Value = Pred->resolve(!NewValue);
}

bool AndPredicate::resolve() {
  for (Predicate *Predicate : Children) {
    if (!Predicate->satisfied())
      return _Value = false;
  }
  return _Value = true;
}

bool AndPredicate::resolve(bool NewValue) {
  for (Predicate *Predicate : Children) {
    Predicate->resolve(NewValue);
  }
  return _Value = true;
}

bool OrPredicate::resolve() {
  for (Predicate *C : Children) {
    if (C->satisfied())
      return _Value = true;
  }
  return _Value = false;
}

bool OrPredicate::resolve(bool NewValue) {
  if (_Value == NewValue)
    return _Value;
  if (NewValue)
    return Children[0]->resolve(true);
  for (Predicate *C : Children)
    C->resolve(false);
  return false;
}

PredicateKeeper::PredicateKeeper() {
  AllPredicates.push_back(True);
  AllPredicates.push_back(False);
}

PredicateKeeper::~PredicateKeeper() {
  for (Predicate *P : AllPredicates) {
    if (P)
      delete P;
  }
}

Predicate *PredicateKeeper::name(const std::string &Name) const {
  if (Name == "TruePredicate")
    return True;
  if (Name == "FalsePredicate")
    return False;
  return NamedPredicates[NamedPredLookup.at(
      IsCaseSensitive ? Name : StringRef(Name).lower())];
}

void PredicateKeeper::resolve() {
  for (Predicate *P : AllPredicates) {
    if (!P)
#ifndef NDEBUG
      report_fatal_error("Nullptr found in predicate list");
#else
      continue;
#endif
    P->resolve();
  }
  Dirty = false;
}

void PredicateKeeper::enable(const std::string &Name) {
  name(Name)->resolve(true);
  Dirty = true;
}

void PredicateKeeper::enable(size_t I) {
  name(I)->resolve(true);
  Dirty = true;
}

void PredicateKeeper::disable(const std::string &Name) {
  name(Name)->resolve(false);
  Dirty = true;
}

void PredicateKeeper::disable(size_t I) {
  name(I)->resolve(false);
  Dirty = true;
}

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
      errs() << "FATAL ERROR: Failed to extract condition for predicate "
             << Name << "\n";
      if (Verbosity)
        errs() << Record << "\n";
      exit(1);
    }

    std::string CondString = Match.str(1);
    if (CondString.empty()) {
      errs() << "FATAL ERROR: Got empty condition for predicate " << Name
             << "\n";
      if (Verbosity)
        errs() << Record << "\n";
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
      errs() << "FATAL ERROR: Failed to parse condition for predicate " << Name
             << ".\n";
      if (Verbosity)
        errs() << "CondString = " << CondString << "\n";
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
  resolve();
}

void PredicateKeeper::updatePatternPredicates(
    const std::vector<bool> &NewPatternPredicates) {
  CustomizedPatternPredicates = true;
  for (size_t i = 0; i < PatternPredicates.size(); i++) {
    // Try to update named predicate values.
    // This can (and will) be utterly inaccurate since we can't be sure which
    // child of an OrPredicate should have been true if PatPred is true.
    PatternPredicates[i]->resolve(NewPatternPredicates[i]);
    // Replace the original Predicate with the confirmed value.
    // The old one will get deleted in dtor (still in AllPredicates).
    PatternPredicates[i] = NewPatternPredicates[i] ? True : False;
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
    if (Verbosity > 1)
      errs() << "WARNING: Found unnamed predicate literal: " << Expr << ".\n";
    Predicate *P = new LiteralPredicate(false);
    AllPredicates.push_back(P);
    std::string LookupName = IsCaseSensitive ? Expr : StringRef(Expr).lower();
    NamedPredLookup.insert(std::make_pair(LookupName, NamedPredicates.size()));
    NamedPredicates.push_back(P);
    LiteralExpressions.insert(std::make_pair(Expr, LookupName));
  }
  return name(LiteralExpressions.at(Expr));
}

// Helper function to report parsing errors and exit program
[[noreturn]] void PredicateKeeper::notFound(const std::string &Str,
                                            const std::string &CondString,
                                            size_t CurIndex) {
  errs() << "FATAL ERROR: Expected `" << Str << "` at char " << CurIndex + 1
         << " in '" << CondString << "'.\n";
  exit(1);
}