#include "scic/codegen/target.hpp"

#include <cstddef>

#include "scic/codegen/listing.hpp"
#include "scic/codegen/output.hpp"

namespace codegen {
namespace {
class Sci11Strategy : public SciTargetStrategy {
 public:
  int NumArgsSize() const override { return 1; }
  void ListNumArgs(ListingFile* listFile, std::size_t offset,
                   int num_args) const override {
    listFile->ListByte(offset, num_args);
  }
  void WriteNumArgs(OutputWriter* out, int num_args) const override {
    out->WriteByte(num_args);
  }

  bool SupportsDebugInstructions() const override { return false; }
};

class Sci2Strategy : public SciTargetStrategy {
 public:
  int NumArgsSize() const override { return 1; }
  void ListNumArgs(ListingFile* listFile, std::size_t offset,
                   int num_args) const override {
    listFile->ListByte(offset, num_args);
  }
  void WriteNumArgs(OutputWriter* out, int num_args) const override {
    out->WriteWord(num_args);
  }
  bool SupportsDebugInstructions() const override { return true; }
};

}  // namespace

SciTargetStrategy const* SciTargetStrategy::GetSci11() {
  static Sci11Strategy const* strategy = new Sci11Strategy();
  return strategy;
}

SciTargetStrategy const* SciTargetStrategy::GetSci2() {
  static Sci2Strategy const* strategy = new Sci2Strategy();
  return strategy;
}

}  // namespace codegen
