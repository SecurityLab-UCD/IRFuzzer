//===--- llvm-isel-fuzzer.cpp - Fuzzer for instruction selection ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Tool to fuzz instruction selection using libFuzzer.
//
//===----------------------------------------------------------------------===//

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
#if LLVM_VERSION_MAJOR >= 14
#include "llvm/MC/TargetRegistry.h"
#else
#include "llvm/Support/TargetRegistry.h"
#endif
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"

#define DEBUG_TYPE "isel-fuzzer"

using namespace llvm;

static codegen::RegisterCodeGenFlags CGF;

static cl::opt<char>
    OptLevel("O",
             cl::desc("Optimization level. [-O0, -O1, -O2, or -O3] "
                      "(default = '-O2')"),
             cl::Prefix, cl::ZeroOrMore, cl::init('2'));

static cl::opt<std::string>
    TargetTriple("mtriple", cl::desc("Override target triple for module"));

static std::unique_ptr<TargetMachine> TM;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  if (Size <= 1)
    // We get bogus data given an empty corpus - ignore it.
    return 0;

  LLVMContext Context;
  auto M = parseAndVerify(Data, Size, Context);
  if (!M) {
    errs() << "error: input module is broken!\n";
    return 0;
  }

  // Set up the module to build for our target.
  M->setTargetTriple(TM->getTargetTriple().normalize());
  M->setDataLayout(TM->createDataLayout());

  // Build up a PM to do instruction selection.
  legacy::PassManager PM;
  TargetLibraryInfoImpl TLII(TM->getTargetTriple());
  PM.add(new TargetLibraryInfoWrapperPass(TLII));
  raw_null_ostream OS;
  TM->addPassesToEmitFile(PM, OS, nullptr, CGFT_Null);
  PM.run(*M);

  return 0;
}

extern "C" LLVM_ATTRIBUTE_USED int LLVMFuzzerInitialize(int *argc,
                                                        char ***argv) {
  EnableDebugBuffering = true;

  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmPrinters();
  InitializeAllAsmParsers();

  // handleExecNameEncodedBEOpts(*argv[0]);
  cl::ParseCommandLineOptions(*argc, *argv);

  if (TargetTriple.empty()) {
    errs() << *argv[0] << ": -mtriple must be specified\n";
    exit(1);
  }

  Triple TheTriple = Triple(Triple::normalize(TargetTriple));

  // Get the target specific parser.
  std::string Error;
  const Target *TheTarget =
      TargetRegistry::lookupTarget(codegen::getMArch(), TheTriple, Error);
  if (!TheTarget) {
    errs() << argv[0] << ": " << Error;
    return 1;
  }

  // Set up the pipeline like llc does.
  std::string CPUStr = codegen::getCPUStr(),
              FeaturesStr = codegen::getFeaturesStr();

  CodeGenOpt::Level OLvl = CodeGenOpt::Default;
  switch (OptLevel) {
  default:
    errs() << argv[0] << ": invalid optimization level.\n";
    return 1;
  case ' ':
    break;
  case '0':
    OLvl = CodeGenOpt::None;
    break;
  case '1':
    OLvl = CodeGenOpt::Less;
    break;
  case '2':
    OLvl = CodeGenOpt::Default;
    break;
  case '3':
    OLvl = CodeGenOpt::Aggressive;
    break;
  }

  TargetOptions Options = codegen::InitTargetOptionsFromCodeGenFlags(TheTriple);
  TM.reset(TheTarget->createTargetMachine(
      TheTriple.getTriple(), CPUStr, FeaturesStr, Options,
      codegen::getExplicitRelocModel(), codegen::getExplicitCodeModel(), OLvl));
  assert(TM && "Could not allocate target machine!");

  return 0;
}
