#ifndef FUNCTION_DEF_H
#define FUNCTION_DEF_H

#include "llvm/ADT/StringRef.h"

#include <filesystem>
#include <string>
#include <vector>

namespace {

using std::string;
using std::vector;
using std::filesystem::path;

struct FunctionDef {
  string RetTy;
  string Name;
  vector<string> FuncAttrs;
  vector<string> ArgTys;
  vector<vector<string>> ArgAttrs;

  /// RetTy;Name;(ArgTy,)*;((ArgAttr )*,)*;(FuncAttr,)*
  /// Let's not worry about attributes for now.
  FunctionDef(string line);
  string ToString() const;
};

typedef vector<FunctionDef> FuncDefs;

FuncDefs ReadFromFile(path P);
void DumpToFile(path P, FuncDefs Defs);

} // namespace

#endif