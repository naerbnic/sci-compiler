#ifndef SEM_PROC_TABLE_HPP
#define SEM_PROC_TABLE_HPP

#include <memory>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/sem/common.hpp"
#include "util/strings/ref_str.hpp"

namespace sem {

class Procedure {
 public:
  virtual ~Procedure() = default;
  virtual NameToken const& token_name() const = 0;
  virtual util::RefStr const& name() const = 0;
  virtual codegen::PtrRef* ptr_ref() const = 0;
};

class ProcTable {
 public:
  virtual ~ProcTable() = default;

  virtual Procedure const* LookupByName(std::string_view name) const = 0;
};

class ProcTableBuilder {
 public:
  static std::unique_ptr<ProcTableBuilder> Create(
      codegen::CodeGenerator* codegen);

  virtual ~ProcTableBuilder() = default;

  virtual absl::Status AddProcedure(NameToken name) = 0;
  virtual absl::StatusOr<std::unique_ptr<ProcTable>> Build() = 0;
};

}  // namespace sem

#endif