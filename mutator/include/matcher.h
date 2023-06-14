#ifndef MATCHER_H
#define MATCHER_H
#include "FunctionDef.h"
#include <filesystem>

namespace {
using std::filesystem::path;
FuncDefs AnalysisMatcherTableCoverage(path MatcherTablePath, path JSONPath);
} // namespace
#endif