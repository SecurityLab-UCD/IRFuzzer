#ifndef INSERT_INTRINSIC_STRATEGY_H
#define INSERT_INTRINSIC_STRATEGY_H
#include "FunctionDef.h"
#include "matcher.h"
#include "llvm/FuzzMutate/IRMutator.h"

#include <filesystem>
#include <string>

namespace llvm {

using std::string;
using std::filesystem::path;

/// Strategy that injects intrinsic definitions.
/// It analysis the matcher table coverage periodically to achieve that.
class InsertIntrinsicStrategy : public IRMutationStrategy {
private:
  path JSONPath;
  path WorkDirPath;

public:
  InsertIntrinsicStrategy(string JSON, string WorkDir)
      : JSONPath(JSON), WorkDirPath(WorkDir) {}
  uint64_t getWeight(size_t CurrentSize, size_t MaxSize,
                     uint64_t CurrentWeight) override {
    return 1;
  }

  /// Only load or analysis when called.
  void LazyInit();

  using IRMutationStrategy::mutate;
  void mutate(Module &M, RandomIRBuilder &IB);
};
} // namespace llvm
#endif