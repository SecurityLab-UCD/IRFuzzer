#include "InsertIntrinsicStrategy.h"
#include "matcher.h"
#include "llvm/FuzzMutate/IRMutator.h"

#include <filesystem>
#include <string>

namespace llvm {

void InsertIntrinsicStrategy::mutate(Module &M, RandomIRBuilder &IB) { return; }

} // namespace llvm