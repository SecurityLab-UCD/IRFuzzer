#ifndef PREDICATE_H_
#define PREDICATE_H_

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
  // Make this predicate take a new value.
  virtual bool resolve(bool NewValue) = 0;

  KindTy getKind() const { return Kind; }

protected:
  // All predicates cache their resolved values.  It is the user's
  // responsibility to determine when to resolve() and when to call satisfied().
  bool _Value = false;
  KindTy Kind;
};

struct LiteralPredicate : Predicate {
  LiteralPredicate(bool Value) : Predicate(Predicate::Literal) {
    _Value = Value;
  }
  bool satisfied() override { return _Value; }
  bool resolve() override { return _Value; }
  bool resolve(bool NewValue) override { return _Value = NewValue; }
};

struct TruePredicate : LiteralPredicate {
  TruePredicate() : LiteralPredicate(true) {}
  bool satisfied() override { return _Value = true; }
  bool resolve() override { return _Value = true; }
  bool resolve(bool) override { return _Value = true; }
};

struct FalsePredicate : LiteralPredicate {
  FalsePredicate() : LiteralPredicate(false) {}
  bool satisfied() override { return _Value = false; }
  bool resolve() override { return _Value = false; }
  bool resolve(bool) override { return _Value = false; }
};

struct NotPredicate : Predicate {
  Predicate *Pred;

  NotPredicate(Predicate *Pred) : Predicate(Predicate::Not), Pred(Pred) {}

  bool resolve() override { return _Value = !Pred->satisfied(); }
  bool resolve(bool NewValue) override;
};

struct AndPredicate : Predicate {
  std::vector<Predicate *> Children;

  AndPredicate(const std::vector<Predicate *> &Predicates)
      : Predicate(Predicate::And), Children(Predicates) {}
  AndPredicate(std::vector<Predicate *> &&Predicates)
      : Predicate(Predicate::And), Children(std::move(Predicates)) {}

  bool resolve() override;
  bool resolve(bool NewValue) override;
};

struct OrPredicate : Predicate {
  std::vector<Predicate *> Children;

  OrPredicate(const std::vector<Predicate *> &Predicates)
      : Predicate(Predicate::Or), Children(Predicates) {}
  OrPredicate(std::vector<Predicate *> &&Predicates)
      : Predicate(Predicate::Or), Children(std::move(Predicates)) {}

  bool resolve() override;
  bool resolve(bool NewValue) override;
};

struct PredicateKeeper {
  friend class MatcherTree;

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

  // Whether named predicates are case-sensitive.
  // If false, all names will be in lower case, and queries will be converted to
  // lower case.
  bool IsCaseSensitive = false;
  // True if pattern predicate values have been supplied from the command line.
  // When true, skip checking named predicates during upper bound calculation.
  bool CustomizedPatternPredicates = false;
  Predicate *True = new TruePredicate();
  Predicate *False = new FalsePredicate();
  size_t Verbosity = 0;

  PredicateKeeper(const PredicateKeeper &) = default;
  PredicateKeeper &operator=(const PredicateKeeper &) = default;

public:
  PredicateKeeper();
  PredicateKeeper(PredicateKeeper &&);
  ~PredicateKeeper();

  // Access a named predicate by name or index
  Predicate *name(const std::string &Name) const;
  Predicate *name(size_t Idx) const { return NamedPredicates[Idx]; }
  // Access a pattern predicate by index
  Predicate *pat(size_t Idx) const { return PatternPredicates[Idx]; }

  // Recalculate all predicate values.
  void resolve();

  auto begin() const { return NamedPredicates.begin(); }
  auto end() const { return NamedPredicates.end(); }

  // Add named predicates before adding pattern predicates
  void addNamedPredicates(const std::vector<std::string> &Records);
  void addPatternPredicates(const std::vector<std::string> &Expressions);
  void updatePatternPredicates(const std::vector<bool> &NewPatternPredicates);

  // Set a named predicate to true orfalse.
  void enable(const std::string &Name);
  void enable(size_t I);
  void disable(const std::string &Name);
  void disable(size_t I);

  bool isDirty() const { return Dirty; }
  size_t getPadPredSize() const { return PatternPredicates.size(); }

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