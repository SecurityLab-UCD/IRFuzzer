#include "lookup.h"
#include "matchertree.h"
#include "shadowmap.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/WithColor.h"
#include <fstream>
#include <set>
#include <sstream>

namespace llvm {

enum class AnalysisMode { UpperBound, Intersect, Diff, Stat };

static cl::OptionCategory AnalysisCategory("Analysis Options");

// ----------------------------------------------------------------
// upperbound subcommand

static cl::SubCommand UBCmd("upperbound",
                            "Calculate matcher table coverage upper bound");

static cl::opt<std::string>
    LookupFilename(cl::Positional, cl::desc("<lookup-table>"), cl::Required,
                   cl::cat(AnalysisCategory), cl::sub(UBCmd));

static cl::list<size_t>
    UBTruePredIndices(cl::Positional, cl::desc("[true-predicate-indices...]"),
                      cl::ZeroOrMore, cl::sub(UBCmd),
                      cl::cat(AnalysisCategory));

static cl::opt<std::string>
    UBOutputFile("o", cl::desc("Generate shadow map output"), cl::Optional,
                 cl::value_desc("outfile"), cl::sub(UBCmd),
                 cl::cat(AnalysisCategory));

// ----------------------------------------------------------------
// intersect subcommand

static cl::SubCommand IntersectCmd("intersect",
                                   "Calculate shadow map intersection");

static cl::opt<size_t> IntTableSize(cl::Positional, cl::desc("<table-size>"),
                                    cl::Required, cl::sub(IntersectCmd),
                                    cl::cat(AnalysisCategory));

static cl::opt<std::string>
    IntLeftMapFilename(cl::Positional, cl::desc("<left-map>"), cl::Required,
                       cl::sub(IntersectCmd), cl::cat(AnalysisCategory));

static cl::opt<std::string>
    IntRightMapFilename(cl::Positional, cl::desc("<right-map>"), cl::Required,
                        cl::sub(IntersectCmd), cl::cat(AnalysisCategory));

static cl::opt<std::string>
    IntOutputFile("o", cl::desc("Generate shadow map output"), cl::Optional,
                  cl::value_desc("outfile"), cl::sub(IntersectCmd),
                  cl::cat(AnalysisCategory));

// ----------------------------------------------------------------
// diff subcommand

static cl::SubCommand DiffCmd("diff", "Calculate shadow map difference");

static cl::opt<size_t> DiffTableSize(cl::Positional, cl::desc("<table-size>"),
                                     cl::Required, cl::sub(DiffCmd),
                                     cl::cat(AnalysisCategory));

static cl::opt<std::string> DiffLeftMapFilename(cl::Positional,
                                                cl::desc("<left-map>"),
                                                cl::Required, cl::sub(DiffCmd),
                                                cl::cat(AnalysisCategory));

static cl::opt<std::string> DiffRightMapFilename(cl::Positional,
                                                 cl::desc("<right-map>"),
                                                 cl::Required, cl::sub(DiffCmd),
                                                 cl::cat(AnalysisCategory));

static cl::opt<std::string>
    DiffOutputFile("o", cl::desc("Generate shadow map output"), cl::Optional,
                   cl::value_desc("outfile"), cl::sub(DiffCmd),
                   cl::cat(AnalysisCategory));

// ----------------------------------------------------------------
// stat subcommand

static cl::SubCommand StatCmd("stat", "Show statistics of shadow map(s)");

static cl::opt<size_t> StatTableSize(cl::Positional, cl::desc("<table-size>"),
                                     cl::Required, cl::sub(StatCmd),
                                     cl::cat(AnalysisCategory));

static cl::list<std::string> StatFiles(cl::Positional, cl::desc("<maps...>"),
                                       cl::OneOrMore, cl::sub(StatCmd),
                                       cl::cat(AnalysisCategory));

// ----------------------------------------------------------------
// subcommand handlers

void handleUBCmd() {
  Expected<json::Value> ParseResult = parseLookupTable(LookupFilename);
  json::Object &LookupTable = *ParseResult.get().getAsObject();

  std::vector<llvm::StringRef> Predicates = getPredicates(LookupTable);
  std::vector<Pattern> Patterns = getPatterns(LookupTable);
  std::vector<Matcher> Matchers = getMatchers(LookupTable);
  size_t TableSize = LookupTable.getInteger("table_size").value();

  // Find true predicate index
  size_t TruePredIdx = 0;
  for (auto It = Predicates.begin(); It != Predicates.end(); It++) {
    if (It->starts_with("TruePredicate ")) {
      TruePredIdx = std::distance(Predicates.begin(), It);
    }
  }

  std::set<size_t> TruePredIndices = {TruePredIdx};
  for (size_t PredIdx : UBTruePredIndices) {
    TruePredIndices.insert(PredIdx);
  }

  MatcherTree TheMatcherTree(Matchers);
  std::vector<bool> ShadowMap(TableSize);
  for (size_t i = 0; i < TableSize; i++) {
    std::set<size_t> PatternsAtIdx = TheMatcherTree.getPatternsAt(i);
    for (size_t Pat : PatternsAtIdx) {
      for (size_t Pred : Patterns[Pat].predicates) {
        if (!TruePredIndices.count(Pred)) {
          // If the predicate is not satisfied, then mark current index as
          // uncovered.
          ShadowMap[i] = true;
          goto NextIdx;
        }
      }
    }
  NextIdx:;
  }
  if (UBOutputFile.getNumOccurrences()) {
    exit(!writeShadowMap(ShadowMap, UBOutputFile.getValue()));
  }

  printShadowMapStats(ShadowMap);
}

void handleDiffCmd() {
  std::vector<bool> Map1 = readShadowMap(DiffTableSize, DiffLeftMapFilename);
  std::vector<bool> Map2 = readShadowMap(DiffTableSize, DiffRightMapFilename);
  std::vector<bool> Map3(Map1.size(), true);
  for (size_t i = 0; i < Map1.size(); i++) {
    if (!Map1[i] && Map2[i]) {
      Map3[i] = false;
    }
  }
  if (DiffOutputFile.getNumOccurrences()) {
    writeShadowMap(Map3, DiffOutputFile);
  } else {
    printShadowMapStats(Map3, "Map diff");
  }
}

void handleIntersectCmd() {
  std::vector<bool> Map1 = readShadowMap(IntTableSize, IntLeftMapFilename);
  std::vector<bool> Map2 = readShadowMap(IntTableSize, IntRightMapFilename);
  std::vector<bool> Map3(Map1.size(), true);
  for (size_t i = 0; i < Map1.size(); i++) {
    if (!Map1[i] && !Map2[i]) {
      Map3[i] = false;
    }
  }
  if (IntOutputFile.getNumOccurrences()) {
    writeShadowMap(Map3, IntOutputFile.getValue());
  } else {
    printShadowMapStats(Map3, "Map intersection");
  }
}

void handleStatCmd() {
  for (const std::string &Filename : StatFiles) {
    std::vector<bool> ShadowMap = readShadowMap(StatTableSize, Filename);
    printShadowMapStats(ShadowMap, "Coverage", Filename);
  }
}

} // end namespace llvm

int main(int argc, char const *argv[]) {
  using namespace llvm;

  InitLLVM X(argc, argv);
  cl::HideUnrelatedOptions({&AnalysisCategory, &getColorCategory()});
  cl::ParseCommandLineOptions(argc, argv, "Shadow map analyzer\n");

  if (UBCmd) {
    handleUBCmd();
  } else if (DiffCmd) {
    handleDiffCmd();
  } else if (IntersectCmd) {
    handleIntersectCmd();
  } else if (StatCmd) {
    handleStatCmd();
  } else {
    llvm_unreachable("No subcommand specified.");
  }
  return 0;
}
