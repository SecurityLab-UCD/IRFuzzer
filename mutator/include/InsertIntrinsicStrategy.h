#ifndef INSERT_INTRINSIC_STRATEGY_H
#define INSERT_INTRINSIC_STRATEGY_H
#include "llvm/FuzzMutate/IRMutator.h"

#include <filesystem>
#include <string>

namespace llvm {

using std::string;
using std::filesystem::path;

/// Strategy that injects intrinsic definitions.
/// It analysis the matcher table coverage periodically to achieve that.
class InsertIntrinsicStrategy : public InsertFunctionStrategy {
private:
  path JSONPath;
  path WorkDirPath;
  std::vector<Intrinsic::ID> IIDs;

public:
  InsertIntrinsicStrategy(const string &JSON, const string &WorkDir)
      : JSONPath(JSON), WorkDirPath(WorkDir) {}
  uint64_t getWeight(size_t CurrentSize, size_t MaxSize,
                     uint64_t CurrentWeight) override {
    return 1;
  }

  /// Only load or analyzes when called.
  void LazyInit(LLVMContext &Context);

  using IRMutationStrategy::mutate;
  Function *chooseFunction(Module *M, RandomIRBuilder &IB) override;
};
} // namespace llvm
#endif