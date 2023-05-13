#ifndef PREDICATE_H_
#define PREDICATE_H_

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorHandling.h"
#include <regex>
#include <unordered_map>
#include <vector>

class PredicateKeeper;

struct Predicate {
  friend PredicateKeeper;

  enum KindTy { Literal, Not, And, Or };

  Predicate(KindTy Kind) : Kind(Kind) {}
  Predicate() = delete;

  // Note that no predicate should take ownership of
  // other predicates. Predicate memory is managed by
  // PredicateKeeper.
  virtual ~Predicate() {}

  virtual bool satisfied() { return _Value; }
  // Recalculate value after literals have been updated.
  virtual bool resolve() = 0;

  KindTy getKind() const { return Kind; }

protected:
  // All predicates cache their resolved values.
  // It is the user's responsibility to determine when
  // to resolve() and when to satisfied(). Only LiteralPredicate
  // resolves upon initialization.
  bool _Value = false;
  KindTy Kind;
};

struct LiteralPredicate : Predicate {
  LiteralPredicate(bool Value) : Predicate(Predicate::Literal) {
    _Value = Value;
  }
  bool satisfied() override { return _Value; }
  bool resolve() override { return _Value; }
};

struct TruePredicate : LiteralPredicate {
  TruePredicate() : LiteralPredicate(true) {}
  bool satisfied() override { return _Value = true; }
  bool resolve() override { return _Value = true; }
};

struct FalsePredicate : LiteralPredicate {
  FalsePredicate() : LiteralPredicate(false) {}
  bool satisfied() override { return _Value = false; }
  bool resolve() override { return _Value = false; }
};

struct NotPredicate : Predicate {
  Predicate *Pred;

  NotPredicate(Predicate *Pred) : Predicate(Predicate::Not), Pred(Pred) {}
  bool resolve() override { return _Value = !Pred->satisfied(); }
};

struct AndPredicate : Predicate {
  std::vector<Predicate *> Children;

  AndPredicate(const std::vector<Predicate *> &Predicates)
      : Predicate(Predicate::And), Children(Predicates) {}
  AndPredicate(std::vector<Predicate *> &&Predicates)
      : Predicate(Predicate::And), Children(std::move(Predicates)) {}

  bool resolve() override {
    for (Predicate *Predicate : Children) {
      if (!Predicate->satisfied())
        return _Value = false;
    }
    return _Value = true;
  }
};

struct OrPredicate : Predicate {
  std::vector<Predicate *> Children;

  OrPredicate(const std::vector<Predicate *> &Predicates)
      : Predicate(Predicate::Or), Children(Predicates) {}
  OrPredicate(std::vector<Predicate *> &&Predicates)
      : Predicate(Predicate::Or), Children(std::move(Predicates)) {}

  bool resolve() override {
    for (Predicate *Predicate : Children) {
      if (Predicate->satisfied())
        return _Value = true;
    }
    return _Value = false;
  }
};

class PredicateKeeper {
  friend class LookupTable;

  // Used for memory management
  std::vector<Predicate *> AllPredicates;
  // Parsed from record dump (.predicates)
  std::vector<Predicate *> NamedPredicates;
  // Predicate name to NamedPredicates index. MapVector doesn't like strings.
  std::unordered_map<std::string, size_t> NamedPredLookup;
  // C++ expression -> predicate name (named predicates only)
  std::unordered_map<std::string, std::string> LiteralExpressions;
  // Parsed from C++ expressions (.pat_predicates)
  std::vector<Predicate *> PatternPredicates;

public:
  Predicate *True = new TruePredicate();
  Predicate *False = new FalsePredicate();

  PredicateKeeper() {
    AllPredicates.push_back(True);
    AllPredicates.push_back(False);
  }
  // Whether named predicates are case-sensitive.
  // If false, all names will be in lower case, and queries will be converted to
  // lower case.
  bool IsCaseSensitive = false;

  // Access a named predicate by name
  Predicate *name(const std::string &Name) const {
    if (Name == "TruePredicate")
      return True;
    if (Name == "FalsePredicate")
      return False;
    return NamedPredicates[NamedPredLookup.at(
        IsCaseSensitive ? Name : llvm::StringRef(Name).lower())];
  }

  // Access a named predicate by insertion index
  Predicate *name(size_t Idx) const { return NamedPredicates[Idx]; }

  Predicate *pat(size_t Idx) const { return PatternPredicates[Idx]; }

  void resolve() {
    for (Predicate *P : AllPredicates) {
      if (!P)
#ifndef NDEBUG
        llvm::report_fatal_error("Nullptr found in predicate list");
#else
        continue;
#endif
      P->resolve();
    }
    Dirty = false;
  }

  auto begin() const { return NamedPredicates.begin(); }
  auto end() const { return NamedPredicates.end(); }

  void addNamedPredicates(const std::vector<std::string> &Records);
  void addPatternPredicates(const std::vector<std::string> &Expressions);

  void enable(const std::string &Name) {
    name(Name)->_Value = true;
    Dirty = true;
  }
  void enable(size_t I) {
    name(I)->_Value = true;
    Dirty = true;
  }
  void disable(const std::string &Name) {
    name(Name)->_Value = false;
    Dirty = true;
  }
  void disable(size_t I) {
    name(I)->_Value = false;
    Dirty = true;
  }

  ~PredicateKeeper() {
    for (Predicate *P : AllPredicates) {
      if (P)
        delete P;
    }
  }

  bool isDirty() const { return Dirty; }

private:
  // Whether or not predicate literals have been modified. For debugging
  // purposes only.
  bool Dirty = false;

  // Poor man's parser for simple C++ expressions found in predicates
  // We don't need tokenization because TableGen's formatting is neat.
  // Can't be static because we need to add new predicates to AllPredicates
  Predicate *parsePredicate(const std::string &CondString);
  Predicate *parseExpr(const std::string &CondString, size_t &CurIndex);
  Predicate *parseGroup(const std::string &CondString, size_t &CurIndex);
  Predicate *parseOr(const std::string &CondString, size_t &CurIndex);
  Predicate *parseAnd(const std::string &CondString, size_t &CurIndex);
  Predicate *parseNot(const std::string &CondString, size_t &CurIndex);
  Predicate *parseLiteral(const std::string &CondString, size_t &CurIndex);
  [[noreturn]] void notFound(const std::string &Str,
                             const std::string &CondString, size_t CurIndex);

  // We don't have tokenization and don't want to parse C++ expressions the hard
  // way. Take advantage of whitespace in TableGen's generated expressions.
  std::string IdentifierRegex = "[A-Za-z_][A-Za-z0-9_]*";
  // HACK: Pray that TableGen doesn't pass function calls as arguments to
  // function calls
  std::string MaybeFuncCallRegex = IdentifierRegex + "(\\(.*?\\))?";
  std::string NoSpaceValueRegex = "(" + IdentifierRegex + "::)?" +
                                  MaybeFuncCallRegex + "((->|.)" +
                                  MaybeFuncCallRegex + ")*";
  std::string MaybeComparisonRegex =
      NoSpaceValueRegex + "( (==|!=) " + NoSpaceValueRegex + ")?";
  std::regex MatchLiteral{"^" + MaybeComparisonRegex};
};

#endif // PREDICATE_H_