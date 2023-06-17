#include "commandline.h"
#include "matchertree.h"
#include "shadowmap.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/WithColor.h"
#include <fstream>

namespace llvm {

void printAnalysisResults(const MatcherTree &MT, size_t EntriesLimit) {
  MapStatPrinter MSP;

  MSP.limit(EntriesLimit);
  outs() << "Top coverage loss cause by matcher kind:\n";
  for (const auto &[Kind, Loss] : MT.blameMatcherKinds()) {
    if (MSP.atLimit())
      break;
    MSP.addStat(Matcher::getKindAsString(Kind), Loss, MT.MatcherTableSize);
  }
  MSP.sum();
  MSP.print();
  outs() << '\n';

  outs() << "Loss from pattern predicate indices:\n";
  for (const auto &[PatPredIdx, Loss] : MT.blamePatternPredicates()) {
    if (MSP.atLimit())
      break;
    MSP.addStat(std::to_string(PatPredIdx), Loss, MT.MatcherTableSize);
  }
  MSP.sum();
  MSP.print();
  outs() << '\n';

  outs() << "Loss by depth:\n";
  for (const auto &[Depth, Loss] : MT.blameDepth()) {
    if (MSP.atLimit())
      break;
    MSP.addStat(std::to_string(Depth), Loss, MT.MatcherTableSize);
  }
  MSP.sum();
  MSP.print();
  outs() << '\n';

  outs() << "Loss of SwitchOpcodeCase by depth:\n";
  for (const auto &[Depth, Loss] : MT.blameSOCAtDepth()) {
    if (MSP.atLimit())
      break;
    MSP.addStat(std::to_string(Depth), Loss, MT.MatcherTableSize);
  }
  MSP.sum();
  MSP.print();
}

size_t getVerbosity(const cl::opt<bool> &VerbosityCL,
                    const cl::opt<std::string> &OutputFileCL) {
  return 1 + VerbosityCL.getNumOccurrences() - OutputFileCL.getNumOccurrences();
}

void handleAnalyzeCmd() {
  size_t Verbosity = getVerbosity(AnVerbosity, AnPatOutFile);
  MatcherTree MT(AnLookupFile, false, Verbosity);
  std::vector<bool> ShadowMap = readBitVector(MT.MatcherTableSize, AnMapFile);
  MT.analyzeMap(ShadowMap);
  if (Verbosity) {
    MapStatPrinter MSP;
    MSP.addFile(AnMapFile, ShadowMap);
    MSP.print();
    outs() << '\n';
    printAnalysisResults(MT, AnMaxEntries);
  }
  if (AnPatOutFile.getNumOccurrences()) {
    std::ofstream PatOfs(AnPatOutFile);
    for (const auto &[Loss, BlameeIdx, BlameeDepth, BlameeKind, Pat] :
         MT.blamePatterns(AnPatUseLossPerPattern)) {
      PatOfs << Loss << ',' << BlameeIdx << ',' << BlameeDepth << ','
             << BlameeKind << ",\"" << Pat << "\"\n";
    }
  }
}

void handleUBCmd() {
  size_t Verbosity = getVerbosity(UBVerbosity, UBOutputFile);
  MatcherTree MT(UBLookupFile, UBPredCaseSensitive, Verbosity);

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
      MT.Predicates.enable(Idx);
      TruePredicates[Idx] = "";
    } else {
      MT.Predicates.enable(Pred);
      Idx = MT.Predicates.NamedPredLookup[MT.Predicates.IsCaseSensitive
                                              ? Pred
                                              : StringRef(Pred).lower()];
      TruePredicates[Idx] = Pred;
    }
  }
  MT.Predicates.resolve();
  if (Verbosity) {
    for (const auto &[I, Name] : TruePredicates) {
      if (!MT.Predicates.name(I)->satisfied()) {
        errs() << "ERROR: Failed to satisfy named predicate " << I;
        if (Name.size())
          errs() << " (" << Name << ")";
        errs() << ".\n";
      }
    }
  }

  if (UBPatPredStr.getNumOccurrences()) {
    std::vector<bool> NewPatPreds;
    size_t PatPredCount = MT.Predicates.PatternPredicates.size();
    if (UBPatPredStr.getValue().size() == PatPredCount) {
      for (size_t i = 0; i < PatPredCount; i++) {
        NewPatPreds.push_back(NewPatPreds[i] == '1');
      }
    } else {
      NewPatPreds = readBitVector(PatPredCount, UBPatPredStr);
    }
    MT.Predicates.updatePatternPredicates(NewPatPreds);
  }

  MT.analyzeUpperBound();
  if (Verbosity) {
    MapStatPrinter MSP("Upper bound");
    MSP.addMap(MT.ShadowMap);
    MSP.print();
    outs() << '\n';
    printAnalysisResults(MT, UBMaxBlameEntries);
  }
  if (UBOutputFile.getNumOccurrences()) {
    exit(!writeBitVector(MT.ShadowMap, UBOutputFile.getValue()));
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