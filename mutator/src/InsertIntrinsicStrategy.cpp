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
  // TODO: Analysis the arg kind and come up with corresponding type.
  auto RS = makeSampler(IB.Rand, make_filter_range(IIDs, [](Intrinsic::ID ID) {
                          return !Intrinsic::isOverloaded(ID);
                        }));
  if (RS.isEmpty())
    return nullptr;
  Intrinsic::ID IID = RS.getSelection();
  Function *F = Intrinsic::getDeclaration(M, IID);
  // AMX can't be stored to memory and has its own syntax and stuff.
  // Too complicated for now, will just ignore it.
  if (F->getReturnType()->isX86_AMXTy() ||
      std::any_of(F->arg_begin(), F->arg_end(),
                  [](Argument &Arg) { return Arg.getType()->isX86_AMXTy(); }))
    return nullptr;
  return F;
}

static std::chrono::nanoseconds GetLastModDuration(const fs::path &FP) {
  if (!fs::is_regular_file(FP))
    return std::chrono::nanoseconds::max();
  return fs::file_time_type::clock::now() - fs::last_write_time(FP);
}

void InsertIntrinsicStrategy::LazyInit() {
  fs::path SavedIIDsPath = WorkDirPath / "saved_iids";
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