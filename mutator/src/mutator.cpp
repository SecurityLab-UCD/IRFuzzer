#include "mutator.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/CodeGen/CommandFlags.h"
#include "llvm/FuzzMutate/FuzzerCLI.h"
#include "llvm/FuzzMutate/IRMutator.h"
#include "llvm/FuzzMutate/Operations.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"

#include <fstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#ifdef DEBUG
#include "llvm/IR/Verifier.h"
#include "llvm/Transforms/Utils/Cloning.h"
#endif
using namespace llvm;

static std::unique_ptr<IRMutator> Mutator;

extern "C" {

void addVectorTypeGetters(std::vector<TypeGetter> &Types) {
  int VectorLength[] = {1, 2, 4, 8, 16, 32, 64};
  std::vector<TypeGetter> BasicTypeGetters(Types);
  for (auto typeGetter : BasicTypeGetters) {
    for (int length : VectorLength) {
      Types.push_back([typeGetter, length](LLVMContext &C) {
        return VectorType::get(typeGetter(C), length, false);
      });
    }
  }
}
/// TODO:
/// Type* getStructType(Context& C);

void createISelMutator() {
  std::vector<TypeGetter> Types{
      Type::getInt1Ty,  Type::getInt8Ty,  Type::getInt16Ty, Type::getInt32Ty,
      Type::getInt64Ty, Type::getFloatTy, Type::getDoubleTy};
  if (!getenv("NO_VEC"))
    addVectorTypeGetters(Types);

  std::vector<std::unique_ptr<IRMutationStrategy>> Strategies;
  std::vector<fuzzerop::OpDescriptor> Ops = InjectorIRStrategy::getDefaultOps();

  Strategies.emplace_back(new InjectorIRStrategy(std::move(Ops)));
  Strategies.emplace_back(new CFGIRStrategy());
  Strategies.emplace_back(new InstDeleterIRStrategy());
  Strategies.emplace_back(new InsertPHItrategy());
  Strategies.emplace_back(new OperandMutatorstrategy());

  Mutator =
      std::make_unique<IRMutator>(std::move(Types), std::move(Strategies));
}

size_t LLVMFuzzerCustomMutator(uint8_t *Data, size_t Size, size_t MaxSize,
                               unsigned int Seed) {
  LLVMContext Context;
  std::unique_ptr<Module> M;
  if (Size <= 1)
    // We get bogus data given an empty corpus - just create a new module.
    M.reset(new Module("M", Context));
  else
    M = parseModule(Data, Size, Context);
  if (!M) {
    errs() << "Parse module error. No mutation is done. Data size: " << Size
           << ". Given data wrote to err.bc\n";
    std::ofstream outfile =
        std::ofstream("err.bc", std::ios::out | std::ios::binary);
    outfile.write((char *)Data, Size);
    outfile.close();
#ifdef DEBUG
    exit(1);
#else
    // We don't do any change.
    return Size;
#endif
  }
#ifdef DEBUG
  std::unique_ptr<Module> OldM = CloneModule(*M);
#endif

  srand(Seed);
  for (int i = 0; i < 4; i++) {
    Mutator->mutateModule(*M, rand(), Size, MaxSize);
  }

#ifdef DEBUG
  uint8_t *OldData = Data;
  uint8_t NewData[MaxSize];
  size_t OldSize = Size;
  size_t NewSize = writeModule(*M, NewData, MaxSize);
  LLVMContext NewC;
  auto NewM = parseModule(NewData, NewSize, NewC);
  if (NewM == nullptr) {
    time_t seconds = time(NULL);
    errs() << "Verification failed, seed: " << Seed << "\n";
    char filename[256];
    memset(filename, 0, 256);
    sprintf(filename, "%u-%zu", Seed, seconds);
    std::ofstream oldoutfile =
        std::ofstream(filename, std::ios::out | std::ios::binary);
    oldoutfile.write((char *)OldData, OldSize);
    oldoutfile.close();
    return OldSize;
  } else {
    memset(OldData, 0, MaxSize);
    memcpy(OldData, NewData, NewSize);
    return NewSize;
  }
#else
  return writeModule(*M, Data, MaxSize);
#endif
}
}