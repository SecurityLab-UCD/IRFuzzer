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

static cl::opt<bool> UBVerbosity("v", cl::desc("Set verbosity (-v, -vv, -vvv)"),
                                 cl::init(false), cl::sub(UBCmd),
                                 cl::cat(AnalysisCategory),
                                 cl::ValueDisallowed);

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

static cl::opt<bool> IntVerbosity("v", cl::desc("Increase verbosity"),
                                  cl::init(false), cl::sub(IntersectCmd),
                                  cl::cat(AnalysisCategory),
                                  cl::ValueDisallowed);

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

static cl::opt<bool> DiffVerbosity("v", cl::desc("Increase verbosity"),
                                   cl::init(false), cl::sub(DiffCmd),
                                   cl::cat(AnalysisCategory),
                                   cl::ValueDisallowed);

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

static cl::opt<bool> UnionVerbosity("v", cl::desc("Increase verbosity"),
                                    cl::init(false), cl::sub(UnionCmd),
                                    cl::cat(AnalysisCategory),
                                    cl::ValueDisallowed);

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

size_t getVerbosity(const cl::opt<bool> &VerbosityCL,
                    const cl::opt<std::string> &OutputFileCL) {
  if (VerbosityCL.getNumOccurrences()) {
    return VerbosityCL.getNumOccurrences();
  } else if (OutputFileCL.getNumOccurrences()) {
    return 0;
  } else {
    return 1;
  }
}

void handleUBCmd() {
  size_t Verbosity = getVerbosity(UBVerbosity, UBOutputFile);
  LookupTable Table =
      LookupTable::fromFile(UBLookupFile, UBPredCaseSensitive, Verbosity);

  // Load true predicate indices

  // index -> name
  std::map<size_t, std::string> TruePredicates;
  for (const std::string &Pred : UBTruePredicates) {
    if (Pred.empty())
      continue;
    auto NotNumeric = [](unsigned char c) { return !std::isdigit(c); };
    auto FirstNonDigit = std::find_if(Pred.begin(), Pred.end(), NotNumeric);

    // TODO: gracefully exit if index or name does not exist
    size_t Idx = 0;
    if (FirstNonDigit == Pred.end()) {
      Idx = std::stoul(Pred);
      Table.PK.enable(Idx);
      std::string Name;
      // Maybe we should use a bimap?
      for (const auto &[PredName, PredIdx] : Table.PK.NamedPredLookup) {
        if (Idx == PredIdx) {
          Name = PredName;
        }
      }
      TruePredicates[Idx] = Name;
    } else {
      Table.PK.enable(Pred);
      Idx = Table.PK.NamedPredLookup[Pred];
      TruePredicates[Idx] =
          UBPredCaseSensitive ? Pred : StringRef(Pred).lower();
    }
  }
  Table.PK.resolve();
  for (const auto &[I, Name] : TruePredicates) {
    if (!Table.PK.name(I)->satisfied()) {
      if (Verbosity)
        errs() << "ERROR: Failed to satisfy named predicate " << I << " ("
               << Name << ").\n";
    }
  }

  MatcherTree TheMatcherTree(Table);
  auto [UpperBound, ShadowMap, BlameMap] = TheMatcherTree.getUpperBound();
  if (Verbosity || UBShowBlameList) {
    printShadowMapStats(UpperBound, Table.MatcherTableSize, "Upper bound");
  }
  if (UBShowBlameList) {
    size_t N = 0;
    outs() << '\n';
    outs() << "Loss from pattern predicate indices";
    if (UBMaxBlameEntries.getNumOccurrences())
      outs() << " (top " << UBMaxBlameEntries.getValue() << ")";
    outs() << ":\n";
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
    outs() << '\n';
    printShadowMapStats(LossSum, Table.MatcherTableSize, "Sum");
  }
  if (UBOutputFile.getNumOccurrences()) {
    exit(!writeShadowMap(ShadowMap, UBOutputFile.getValue()));
  }
}

void handleDiffCmd() {
  auto Op = [](bool R, bool M) { return R | !M; };
  bool PrintOutput = getVerbosity(DiffVerbosity, DiffOutputFile);
  std::vector<bool> ResultMap = doMapOp(DiffTableSize, DiffFiles.begin(),
                                        DiffFiles.end(), Op, PrintOutput);
  if (PrintOutput) {
    printShadowMapStats(ResultMap, "Map difference");
  }
  if (DiffOutputFile.getNumOccurrences()) {
    exit(!writeShadowMap(ResultMap, DiffOutputFile));
  }
}

void handleIntersectCmd() {
  auto Op = [](bool R, bool M) { return R | M; };
  bool PrintOutput = getVerbosity(IntVerbosity, IntOutputFile);
  std::vector<bool> ResultMap =
      doMapOp(IntTableSize, IntFiles.begin(), IntFiles.end(), Op, PrintOutput);
  if (PrintOutput) {
    printShadowMapStats(ResultMap, "Map intersection");
  }
  if (IntOutputFile.getNumOccurrences()) {
    exit(!writeShadowMap(ResultMap, IntOutputFile.getValue()));
  }
}

void handleUnionCmd() {
  auto Op = [](bool R, bool M) { return R & M; };
  bool PrintOutput = getVerbosity(UnionVerbosity, UnionOutputFile);
  std::vector<bool> ResultMap = doMapOp(UnionTableSize, UnionFiles.begin(),
                                        UnionFiles.end(), Op, PrintOutput);
  if (PrintOutput) {
    printShadowMapStats(ResultMap, "Map union");
  }
  if (UnionOutputFile.getNumOccurrences()) {
    exit(!writeShadowMap(ResultMap, UnionOutputFile));
  }
}

void handleStatCmd() {
  for (const std::string &Filename : StatFiles) {
    std::vector<bool> ShadowMap = readShadowMap(StatTableSize, Filename);
    printShadowMapStats(ShadowMap, "", Filename);
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
