#include "commandline.h"
#include "lookup.h"
#include "matchertree.h"
#include "shadowmap.h"

#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/WithColor.h"

namespace llvm {

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
  MSP.limit(AnMaxEntries);
  for (const auto &[Kind, Loss] : MatcherBlame) {
    if (MSP.atLimit())
      break;
    // TODO: display matcher kind enum names instead of indices
    MSP.addStat(Matcher::getKindAsString(Kind), Loss, ShadowMap.size());
    LossSum += Loss;
  }
  MSP.summarize("Sum", LossSum, ShadowMap.size(), true);
  MSP.print();
  outs() << '\n';

  outs() << "Loss from pattern predicate indices:\n";
  LossSum = 0;
  MSP.limit(AnMaxEntries);
  for (const auto &[PatPredIdx, Loss] : PatPredBlame) {
    if (MSP.atLimit())
      break;
    LossSum += Loss;
    MSP.addStat(std::to_string(PatPredIdx), Loss, Table.MatcherTableSize);
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
      if (MSP.atLimit())
        break;
      LossSum += Loss;
      MSP.addStat(std::to_string(PatPredIdx), Loss, Table.MatcherTableSize);
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

} // namespace llvm

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
