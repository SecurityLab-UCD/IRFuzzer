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

static cl::opt<AnalysisMode> AnalysisModeCL(
    cl::desc("Analysis mode"), cl::Required,
    cl::values(clEnumValN(AnalysisMode::UpperBound, "upperbound",
                          "Calculate matcher table coverage upper bound"),
               clEnumValN(AnalysisMode::Intersect, "intersect",
                          "Calculate shadow map intersection"),
               clEnumValN(AnalysisMode::Diff, "diff",
                          "Calculate shadow map difference"),
               clEnumValN(AnalysisMode::Stat, "stat",
                          "Print coverage statistics for given map(s)")),
    cl::cat(AnalysisCategory));

static cl::opt<std::string>
    GenerateShadowMapCL("as-map", cl::desc("Generate analysis as shadow map"),
                        cl::Optional, cl::value_desc("outfile"),
                        cl::cat(AnalysisCategory));

static cl::list<std::string>
    ShadowMapFilenames("map", cl::desc("Shadow map(s) to analyze"),
                       cl::ZeroOrMore, cl::value_desc("infile"),
                       cl::cat(AnalysisCategory));

static cl::opt<std::string>
    LookupFilename("lookup", cl::desc("Path to pattern lookup table"),
                   cl::Optional, cl::value_desc("infile"),
                   cl::cat(AnalysisCategory));

static cl::opt<size_t> TableSizeCL("table-size",
                                   cl::desc("Size of shadow map in bits"),
                                   cl::Optional, cl::value_desc("bits"),
                                   cl::cat(AnalysisCategory));

static cl::list<size_t> TruePredicatesCL("pred",
                                         cl::desc("Indices of true predicates"),
                                         cl::ZeroOrMore, cl::CommaSeparated,
                                         cl::value_desc("indices..."),
                                         cl::cat(AnalysisCategory));

/// @brief Verify commandline arguments
/// @return True if commandline arguments are invalid
bool gotBadArgs() {
  switch (AnalysisModeCL.getValue()) {
  case AnalysisMode::UpperBound:
    if (ShadowMapFilenames.getNumOccurrences() != 0) {
      errs() << "Shadow map unexpected for upper bound analysis.\n";
      return true;
    }
    if (!LookupFilename.getNumOccurrences()) {
      errs() << "Expected lookup table for upper bound analysis.\n";
      return true;
    }
    break;
  case AnalysisMode::Intersect:
  case AnalysisMode::Diff:
    if (!TableSizeCL.getNumOccurrences()) {
      errs() << "Expected table size for shadow map operations.\n";
      return true;
    }
    if (ShadowMapFilenames.getNumOccurrences() != 2) {
      errs() << "Expected 2 shadow maps for shadow map operations.\n";
      return true;
    }
    break;
  case AnalysisMode::Stat:
    if (!TableSizeCL.getNumOccurrences()) {
      errs() << "Expected table size for shadow map operations.\n";
      return true;
    }
    if (ShadowMapFilenames.getNumOccurrences() == 0) {
      errs() << "Expected at least 1 shadow map for printing statistics.\n";
      return true;
    }
    if (GenerateShadowMapCL.getNumOccurrences()) {
      errs() << "Statistics mode cannot produce shadow maps.\n";
      return true;
    }
    break;
  default:
    llvm_unreachable("Unexpected analysis mode");
  }
  return false;
}

void analyzeUpperBound() {
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
  for (size_t PredIdx : TruePredicatesCL) {
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
  if (GenerateShadowMapCL.getNumOccurrences()) {
    exit(!writeShadowMap(ShadowMap, GenerateShadowMapCL.getValue()));
  }

  printShadowMapStats(ShadowMap);
}

} // end namespace llvm

int main(int argc, char const *argv[]) {
  using namespace llvm;

  InitLLVM X(argc, argv);
  cl::HideUnrelatedOptions({&AnalysisCategory, &getColorCategory()});
  cl::ParseCommandLineOptions(argc, argv, "Shadow map analyzer\n");

  if (gotBadArgs())
    return 1;

  switch (AnalysisModeCL.getValue()) {
  case AnalysisMode::UpperBound:
    analyzeUpperBound();
    break;
  case AnalysisMode::Diff: {
    std::vector<bool> Map1 = readShadowMap(TableSizeCL, ShadowMapFilenames[0]);
    std::vector<bool> Map2 = readShadowMap(TableSizeCL, ShadowMapFilenames[1]);
    std::vector<bool> Map3(Map1.size(), true);
    for (size_t i = 0; i < Map1.size(); i++) {
      if (!Map1[i] && Map2[i]) {
        Map3[i] = false;
      }
    }
    if (GenerateShadowMapCL.getNumOccurrences()) {
      writeShadowMap(Map3, GenerateShadowMapCL.getValue());
    } else {
      printShadowMapStats(Map3, "Map diff");
    }
  } break;
  case AnalysisMode::Intersect: {
    std::vector<bool> Map1 = readShadowMap(TableSizeCL, ShadowMapFilenames[0]);
    std::vector<bool> Map2 = readShadowMap(TableSizeCL, ShadowMapFilenames[1]);
    std::vector<bool> Map3(Map1.size(), true);
    for (size_t i = 0; i < Map1.size(); i++) {
      if (!Map1[i] && !Map2[i]) {
        Map3[i] = false;
      }
    }
    if (GenerateShadowMapCL.getNumOccurrences()) {
      writeShadowMap(Map3, GenerateShadowMapCL.getValue());
    } else {
      printShadowMapStats(Map3, "Map intersection");
    }
  } break;
  case AnalysisMode::Stat: {
    for (const std::string &Filename : ShadowMapFilenames) {
      std::vector<bool> ShadowMap =
          readShadowMap(TableSizeCL.getValue(), Filename);
      printShadowMapStats(ShadowMap, "Coverage", Filename);
    }
  } break;
  default:
    llvm_unreachable("Unknown analysis mode");
  }

  return 0;
}
