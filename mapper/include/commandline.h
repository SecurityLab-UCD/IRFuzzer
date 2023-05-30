#pragma once
#ifndef _COMMAND_LINE_H_
#define _COMMAND_LINE_H_

#include "shadowmap.h"
#include "llvm/Support/CommandLine.h"

namespace llvm {

extern cl::OptionCategory AnalysisCategory;

// ----------------------------------------------------------------
// analyze subcommand

extern cl::SubCommand AnalyzeCmd;
extern cl::opt<bool> AnVerbosity;
extern cl::opt<std::string> AnLookupFile;
extern cl::opt<std::string> AnMapFile;
extern cl::opt<size_t> AnMaxEntries;
extern cl::opt<std::string> AnPatOutFile;
extern cl::opt<bool> AnPatUseLossPerPattern;

// ----------------------------------------------------------------
// upperbound subcommand

extern cl::SubCommand UBCmd;
extern cl::opt<bool> UBVerbosity;
extern cl::opt<std::string> UBLookupFile;
extern cl::list<std::string> UBTruePredicates;
extern cl::opt<std::string> UBPatPredStr;
extern cl::opt<std::string> UBOutputFile;
extern cl::opt<bool> UBPredCaseSensitive;
extern cl::opt<bool> UBShowBlameList;
extern cl::opt<size_t> UBMaxBlameEntries;

// ----------------------------------------------------------------
// intersect subcommand

extern cl::SubCommand IntersectCmd;
extern cl::opt<bool> IntVerbosity;
extern cl::opt<size_t> IntTableSize;
extern cl::list<std::string> IntFiles;
extern cl::opt<std::string> IntOutputFile;

// ----------------------------------------------------------------
// diff subcommand

extern cl::SubCommand DiffCmd;
extern cl::opt<bool> DiffVerbosity;
extern cl::opt<size_t> DiffTableSize;
extern cl::list<std::string> DiffFiles;
extern cl::opt<std::string> DiffOutputFile;

// ----------------------------------------------------------------
// union subcommand

extern cl::SubCommand UnionCmd;
extern cl::opt<bool> UnionVerbosity;
extern cl::opt<size_t> UnionTableSize;
extern cl::list<std::string> UnionFiles;
extern cl::opt<std::string> UnionOutputFile;

// ----------------------------------------------------------------
// stat subcommand

extern cl::SubCommand StatCmd;
extern cl::opt<size_t> StatTableSize;
extern cl::list<std::string> StatFiles;
extern cl::opt<MapStatPrinter::SortTy> StatSort;

} // namespace llvm

#endif // _COMMAND_LINE_H_