#include "lookup.h"
#include "matchertree.h"
#include "shadowmap.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/WithColor.h"

namespace llvm {

static cl::OptionCategory AnalysisCategory("Analysis Options");

// ----------------------------------------------------------------
// analyze subcommand

static cl::SubCommand
    AnalyzeCmd("analyze", "Analyze coverage loss in experimental shadow map");

static cl::opt<bool> AnVerbosity("v", cl::desc("Increase verbosity"),
                                 cl::init(false), cl::sub(AnalyzeCmd),
                                 cl::cat(AnalysisCategory),
                                 cl::ValueDisallowed);

static cl::opt<std::string>
    AnLookupFile(cl::Positional, cl::desc("<lookup-table>"), cl::Required,
                 cl::cat(AnalysisCategory), cl::sub(AnalyzeCmd));

static cl::opt<std::string> AnMapFile(cl::Positional, cl::desc("<map>"),
                                      cl::Required, cl::cat(AnalysisCategory),
                                      cl::sub(AnalyzeCmd));

// ----------------------------------------------------------------
// upperbound subcommand

static cl::SubCommand
    UBCmd("upperbound",
          "Calculate matcher table coverage upper bound given true predicates");

static cl::opt<bool> UBVerbosity("v", cl::desc("Increase verbosity"),
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
    UBPatPredStr("p", cl::desc("Manually set pattern predicate values"),
                 cl::init(""), cl::sub(UBCmd), cl::cat(AnalysisCategory));

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
                      cl::value_desc("entries"), cl::sub(UBCmd),
                      cl::init(std::numeric_limits<size_t>::max()),
                      cl::cat(AnalysisCategory));

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

static cl::opt<MapStatPrinter::SortTy>
    StatSort("sort", cl::desc("Sort by covered indices"),
             cl::init(MapStatPrinter::None),
             cl::values(clEnumValN(MapStatPrinter::None, "none", "Do not sort"),
                        clEnumValN(MapStatPrinter::Asc, "asc",
                                   "Sort in ascending order"),
                        clEnumValN(MapStatPrinter::Desc, "desc",
                                   "Sort in descending order")),
             cl::sub(StatCmd), cl::cat(AnalysisCategory));

// ----------------------------------------------------------------
// subcommand handlers

void handleAnalyzeCmd() {
  size_t Verbosity = 1 + AnVerbosity.getNumOccurrences();
  LookupTable Table = LookupTable::fromFile(AnLookupFile, false, Verbosity);
  MatcherTree TheMatcherTree(Table);
  std::vector<bool> ShadowMap =
      readBitVector(Table.MatcherTableSize, AnMapFile);
  auto [MatcherBlame, PatPredBlame] = TheMatcherTree.analyzeMap(ShadowMap);

  MapStatPrinter MSP;
  MSP.addFile(AnMapFile, ShadowMap);
  MSP.print();
  outs() << '\n';

  outs() << "Top coverage loss cause by matcher kind:\n";
  size_t LossSum = 0;
  for (const auto &[Kind, Loss] : MatcherBlame) {
    // TODO: display matcher kind enum names instead of indices
    MSP.addStat(Matcher::getKindAsString(Kind), Loss, ShadowMap.size());
    LossSum += Loss;
  }
  MSP.summarize("Sum", LossSum, ShadowMap.size(), true);
  MSP.print();
  outs() << '\n';

  outs() << "Loss from pattern predicate indices:\n";
  LossSum = 0;
  for (const auto &[PatPredIdx, Loss] : PatPredBlame) {
    LossSum += Loss;
    MSP.addStat(std::to_string(PatPredIdx), Loss, Table.MatcherTableSize);
    if (MSP.atLimit())
      break;
  }
  MSP.summarize("Sum", LossSum, ShadowMap.size(), true);
  MSP.print();
}

size_t getVerbosity(const cl::opt<bool> &VerbosityCL,
                    const cl::opt<std::string> &OutputFileCL) {
  return 1 + VerbosityCL.getNumOccurrences() - OutputFileCL.getNumOccurrences();
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
      TruePredicates[Idx] = "";
    } else {
      Table.PK.enable(Pred);
      Idx = Table.PK.NamedPredLookup[Table.PK.IsCaseSensitive
                                         ? Pred
                                         : StringRef(Pred).lower()];
      TruePredicates[Idx] = Pred;
    }
  }
  Table.PK.resolve();
  if (Verbosity) {
    for (const auto &[I, Name] : TruePredicates) {
      if (!Table.PK.name(I)->satisfied()) {
        errs() << "ERROR: Failed to satisfy named predicate " << I;
        if (Name.size())
          errs() << " (" << Name << ")";
        errs() << ".\n";
      }
    }
  }

  if (UBPatPredStr.getNumOccurrences()) {
    std::vector<bool> NewPatPreds;
    size_t PatPredCount = Table.PK.PatternPredicates.size();
    if (UBPatPredStr.getValue().size() == PatPredCount) {
      for (size_t i = 0; i < PatPredCount; i++) {
        NewPatPreds.push_back(NewPatPreds[i] == '1');
      }
    } else {
      NewPatPreds = readBitVector(PatPredCount, UBPatPredStr);
    }
    Table.PK.updatePatternPredicates(NewPatPreds);
  }

  MatcherTree TheMatcherTree(Table);
  auto [UpperBound, ShadowMap, BlameMap] = TheMatcherTree.getUpperBound();
  if (Verbosity || UBShowBlameList) {
    MapStatPrinter MSP;
    MSP.summarize("Upper bound", UpperBound, ShadowMap.size(), true);
    MSP.print();
  }
  if (UBShowBlameList) {
    outs() << '\n';
    outs() << "Loss from pattern predicate indices";
    if (UBMaxBlameEntries.getNumOccurrences())
      outs() << " (top " << UBMaxBlameEntries.getValue() << ")";
    outs() << ":\n";

    MapStatPrinter MSP;
    MSP.limit(UBMaxBlameEntries);
    size_t LossSum = 0;
    for (const auto &[PatPredIdx, Loss] : BlameMap) {
      LossSum += Loss;
      MSP.addStat(std::to_string(PatPredIdx), Loss, Table.MatcherTableSize);
      if (MSP.atLimit())
        break;
    }
    MSP.summarize("Sum", LossSum, Table.MatcherTableSize, true);
    MSP.print();
  }
  if (UBOutputFile.getNumOccurrences()) {
    exit(!writeBitVector(ShadowMap, UBOutputFile.getValue()));
  }
}

void handleMapOpCmd(const std::string &OpName,
                    const std::vector<std::string> &Files,
                    std::function<bool(bool, bool)> Op, size_t TableSize,
                    const cl::opt<std::string> &OutputFile,
                    const cl::opt<bool> &VerbosityCL) {
  auto Maps = readBitVectors(TableSize, Files);
  std::vector<bool> ResultMap = doMapOp(Maps, Op);
  if (getVerbosity(VerbosityCL, OutputFile)) {
    MapStatPrinter MSP;
    for (size_t I = 0; I < Maps.size(); I++) {
      MSP.addFile(Files[I], Maps[I]);
    }
    MSP.summarize(OpName, ResultMap);
    MSP.print();
  }
  if (OutputFile.getNumOccurrences()) {
    exit(!writeBitVector(ResultMap, OutputFile));
  }
}

void handleDiffCmd() {
  auto Op = [](bool R, bool M) { return R | !M; };
  handleMapOpCmd("Diff", DiffFiles, Op, DiffTableSize, DiffOutputFile,
                 DiffVerbosity);
}

void handleIntersectCmd() {
  auto Op = [](bool R, bool M) { return R | M; };
  handleMapOpCmd("Intersection", IntFiles, Op, IntTableSize, IntOutputFile,
                 IntVerbosity);
}

void handleUnionCmd() {
  auto Op = [](bool R, bool M) { return R & M; };
  handleMapOpCmd("Union", UnionFiles, Op, UnionTableSize, UnionOutputFile,
                 UnionVerbosity);
}

void handleStatCmd() {
  MapStatPrinter MSP;
  for (const std::string &Filename : StatFiles) {
    MSP.addFile(Filename, StatTableSize);
  }
  MSP.sort(StatSort);
  MSP.print();
}

} // end namespace llvm

int main(int argc, char const *argv[]) {
  using namespace llvm;

  InitLLVM X(argc, argv);
  cl::HideUnrelatedOptions({&AnalysisCategory, &getColorCategory()});
  cl::ParseCommandLineOptions(argc, argv, "Shadow map analyzer\n");

  if (AnalyzeCmd) {
    handleAnalyzeCmd();
  } else if (UBCmd) {
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
