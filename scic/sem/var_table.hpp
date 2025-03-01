#ifndef SEM_GLOBAL_TABLE_HPP
#define SEM_GLOBAL_TABLE_HPP

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/sem/common.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/sequence.hpp"

namespace sem {

// A table of global variables.
class VarTable {
 public:
  class Variable {
   public:
    virtual ~Variable() = default;

    virtual NameToken const& token_name() const = 0;
    virtual util::RefStr const& name() const = 0;
    virtual std::size_t index() const = 0;
    virtual std::size_t length() const = 0;
    virtual std::optional<util::Seq<codegen::LiteralValue>> initial_value()
        const = 0;
  };

  virtual ~VarTable() = default;

  virtual util::Seq<Variable const&> vars() const = 0;

  virtual Variable const* LookupByName(std::string_view name) const = 0;
  virtual Variable const* LookupByIndex(std::size_t global_index) const = 0;
};

class VarTableBuilder {
 public:
  static std::unique_ptr<VarTableBuilder> Create();

  virtual ~VarTableBuilder() = default;

  // Adds a variable declaration to this table.
  //
  // If the same variable has been defined before, with both the same name and
  // index, then this is a no-op.
  virtual absl::Status DeclareVar(NameToken name, std::size_t global_index,
                                  std::size_t length) = 0;
  virtual absl::Status DefineVar(NameToken name, std::size_t global_index,
                                 std::vector<codegen::LiteralValue> values) = 0;
  virtual absl::StatusOr<std::unique_ptr<VarTable>> Build() = 0;
};

}  // namespace sem
#endif