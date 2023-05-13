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

static cl::SubCommand
    UBCmd("upperbound",
          "Calculate matcher table coverage upper bound given true predicates");

static cl::opt<std::string>
    UBLookupFile(cl::Positional, cl::desc("<lookup-table>"), cl::Required,
                 cl::cat(AnalysisCategory), cl::sub(UBCmd));

static cl::list<std::string>
    UBTruePredicates(cl::Positional, cl::desc("[true-pred-name-or-idx...]"),
                     cl::ZeroOrMore, cl::sub(UBCmd), cl::cat(AnalysisCategory));

static cl::opt<std::string>
    UBOutputFile("o", cl::desc("Generate shadow map output"), cl::Optional,
                 cl::value_desc("outfile"), cl::sub(UBCmd),
                 cl::cat(AnalysisCategory));

static cl::opt<bool>
    UBPredCaseSensitive("s", cl::desc("Make predicate name case sensitive"),
                        cl::init(false), cl::sub(UBCmd),
                        cl::cat(AnalysisCategory));

static cl::opt<bool>
    UBShowBlameList("b", cl::desc("Show matcher coverage blame list"),
                    cl::init(false), cl::sub(UBCmd), cl::cat(AnalysisCategory));

static cl::opt<size_t>
    UBMaxBlameEntries("l", cl::desc("Limit blame list entries printed"),
                      cl::sub(UBCmd), cl::cat(AnalysisCategory));

// ----------------------------------------------------------------
// intersect subcommand

static cl::SubCommand IntersectCmd("intersect",
                                   "Calculate shadow map intersection");

static cl::opt<size_t> IntTableSize(cl::Positional, cl::desc("<table-size>"),
                                    cl::Required, cl::sub(IntersectCmd),
                                    cl::cat(AnalysisCategory));

static cl::list<std::string> IntFiles(cl::Positional, cl::desc("<maps...>"),
                                      cl::OneOrMore, cl::sub(IntersectCmd),
                                      cl::cat(AnalysisCategory));

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

static cl::list<std::string> DiffFiles(cl::Positional, cl::desc("<maps...>"),
                                       cl::OneOrMore, cl::sub(DiffCmd),
                                       cl::cat(AnalysisCategory));

static cl::opt<std::string>
    DiffOutputFile("o", cl::desc("Generate shadow map output"), cl::Optional,
                   cl::value_desc("outfile"), cl::sub(DiffCmd),
                   cl::cat(AnalysisCategory));

// ----------------------------------------------------------------
// union subcommand

static cl::SubCommand UnionCmd("union", "Calculate shadow map union");

static cl::opt<size_t> UnionTableSize(cl::Positional, cl::desc("<table-size>"),
                                      cl::Required, cl::sub(UnionCmd),
                                      cl::cat(AnalysisCategory));

static cl::list<std::string> UnionFiles(cl::Positional, cl::desc("<maps...>"),
                                        cl::OneOrMore, cl::sub(UnionCmd),
                                        cl::cat(AnalysisCategory));

static cl::opt<std::string>
    UnionOutputFile("o", cl::desc("Generate shadow map output"), cl::Optional,
                    cl::value_desc("outfile"), cl::sub(UnionCmd),
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
  LookupTable Table = LookupTable::fromFile(UBLookupFile, UBPredCaseSensitive);

  // Load true predicate indices
  std::set<size_t> TruePredIndices;
  UBTruePredicates.push_back("TruePredicate");
  for (const std::string &Pred : UBTruePredicates) {
    if (Pred.empty())
      continue;
    auto NotNumeric = [](unsigned char c) { return !std::isdigit(c); };
    auto FirstNonDigit = std::find_if(Pred.begin(), Pred.end(), NotNumeric);

    // TODO: gracefully exit if index or name does not exist
    if (FirstNonDigit == Pred.end()) {
      Table.PK.enable(std::stoi(Pred));
    } else {
      Table.PK.enable(Pred);
    }
  }
  Table.PK.resolve();

  MatcherTree TheMatcherTree(Table);
  auto [UpperBound, ShadowMap, BlameMap] = TheMatcherTree.getUpperBound();
  if (UBOutputFile.getNumOccurrences()) {
    exit(!writeShadowMap(ShadowMap, UBOutputFile.getValue()));
  }
  printShadowMapStats(UpperBound, Table.MatcherTableSize, "Upper bound");
  if (UBShowBlameList.getValue()) {
    size_t N = 0;
    outs() << '\n';
    outs() << "Loss from pattern predicate indices:\n";
    size_t LossSum = 0;
    size_t IdxPadLen = std::to_string(Table.PK.getPadPredSize()).size();
    for (const auto [Loss, PatPredIdx] : BlameMap) {
      LossSum += Loss;
      std::string IdxStr = std::to_string(PatPredIdx);
      IdxStr.insert(IdxStr.begin(), IdxPadLen - IdxStr.size(), ' ');
      printShadowMapStats(Loss, Table.MatcherTableSize, IdxStr);
      N++;
      if (UBMaxBlameEntries.getNumOccurrences() &&
          N == UBMaxBlameEntries.getValue())
        break;
    }
    printShadowMapStats(LossSum, Table.MatcherTableSize, "Total listed loss");
  }
}

void handleDiffCmd() {
  auto Op = [](bool R, bool M) { return R | !M; };
  bool PrintOutput = DiffOutputFile.getNumOccurrences() == 0;
  std::vector<bool> ResultMap = doMapOp(DiffTableSize, DiffFiles.begin(),
                                        DiffFiles.end(), Op, PrintOutput);
  if (PrintOutput) {
    printShadowMapStats(ResultMap, "Map difference");
  } else {
    writeShadowMap(ResultMap, DiffOutputFile);
  }
}

void handleIntersectCmd() {
  auto Op = [](bool R, bool M) { return R | M; };
  bool PrintOutput = IntOutputFile.getNumOccurrences() == 0;
  std::vector<bool> ResultMap =
      doMapOp(IntTableSize, IntFiles.begin(), IntFiles.end(), Op, PrintOutput);
  if (PrintOutput) {
    printShadowMapStats(ResultMap, "Map intersection");
  } else {
    writeShadowMap(ResultMap, IntOutputFile.getValue());
  }
}

void handleUnionCmd() {
  auto Op = [](bool R, bool M) { return R & M; };
  bool PrintOutput = UnionOutputFile.getNumOccurrences() == 0;
  std::vector<bool> ResultMap = doMapOp(UnionTableSize, UnionFiles.begin(),
                                        UnionFiles.end(), Op, PrintOutput);
  if (PrintOutput) {
    printShadowMapStats(ResultMap, "Map union");
  } else {
    writeShadowMap(ResultMap, UnionOutputFile);
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
  } else if (UnionCmd) {
    handleUnionCmd();
  } else {
    errs() << "Please specify a subcommand.\n";
    cl::PrintHelpMessage(false, true);
    return 1;
  }
  return 0;
}
