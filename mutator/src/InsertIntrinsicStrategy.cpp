#include "InsertIntrinsicStrategy.h"
#include "matchertree.h"
#include "shadowmap.h"
#include "llvm/FuzzMutate/IRMutator.h"
#include "llvm/FuzzMutate/Random.h"
#include "llvm/FuzzMutate/RandomIRBuilder.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace llvm {

Function *InsertIntrinsicStrategy::chooseFunction(Module *M,
                                                  RandomIRBuilder &IB) {
  LazyInit();
  auto RS = makeSampler(IB.Rand, IIDs);
  return Intrinsic::getDeclaration(M, RS.getSelection());
}

static std::chrono::nanoseconds GetLastModDuration(const fs::path &FP) {
  if (!fs::is_regular_file(FP))
    return std::chrono::nanoseconds::max();
  return fs::file_time_type::clock::now() - fs::last_write_time(FP);
}

void InsertIntrinsicStrategy::LazyInit() {
  fs::path SavedIIDsPath = WorkDirPath / "saved_iids";
  const auto Threshold = 10min;
  const auto LastMod = GetLastModDuration(SavedIIDsPath);

  if (LastMod <= Threshold) {
    if (IsInitialized)
      return;

    Intrinsic::ID IID;
    std::ifstream Ifs(SavedIIDsPath);
    while (Ifs >> IID) {
      IIDs.push_back(IID);
    }
    IsInitialized = true;
    return;
  }

  // Saved IIDs are stale. Regenerate the list.
  MatcherTree MT(JSONPath);
  fs::path ShadowMapPath = WorkDirPath / "fuzz_shadowmap";
  std::vector<bool> Map;
  if (!fs::is_regular_file(ShadowMapPath)) {
    Map.resize(MT.MatcherTableSize, true);
  } else {
    Map = readBitVector(MT.MatcherTableSize, ShadowMapPath);
  }
  MT.analyzeMap(Map);
  IIDs = MT.blameTargetIntrinsic();

  std::ofstream Ofs(SavedIIDsPath);
  for (Intrinsic::ID IID : IIDs) {
    Ofs << IID << '\n';
  }
  IsInitialized = true;
}

} // namespace llvm