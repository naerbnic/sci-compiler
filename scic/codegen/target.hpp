#ifndef CODEGEN_TARGET_HPP
#define CODEGEN_TARGET_HPP

#include <cstddef>

#include "scic/listing.hpp"
#include "scic/output.hpp"

namespace codegen {

// A strategy to handle differences between target architectures.
//
// The strategy is stateless, and all methods should be const.
class SciTargetStrategy {
 public:
  static SciTargetStrategy const* GetSci11();
  static SciTargetStrategy const* GetSci2();

  virtual ~SciTargetStrategy() = default;

  virtual int NumArgsSize() const = 0;
  virtual void ListNumArgs(ListingFile*, std::size_t offset,
                           int num_args) const = 0;
  virtual void WriteNumArgs(OutputFile* out, int num_args) const = 0;

  virtual bool SupportsDebugInstructions() const = 0;
};

}  // namespace codegen

#endif