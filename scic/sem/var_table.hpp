#ifndef SEM_VAR_TABLE_HPP
#define SEM_VAR_TABLE_HPP

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/sem/common.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/sequence.hpp"

namespace sem {

class VarDeclTable {
 public:
  class Variable {
   public:
    virtual ~Variable() = default;
    virtual NameToken const& token_name() const = 0;
    virtual util::RefStr const& name() const = 0;
    virtual GlobalIndex index() const = 0;
    virtual std::size_t length() const = 0;
  };

  virtual ~VarDeclTable() = default;
  virtual util::Seq<Variable const&> vars() const = 0;
  virtual Variable const* LookupByName(std::string_view name) const = 0;
  virtual Variable const* LookupByIndex(GlobalIndex global_index) const = 0;
};

class VarDeclTableBuilder {
 public:
  static std::unique_ptr<VarDeclTableBuilder> Create();
  virtual ~VarDeclTableBuilder() = default;

  // Adds a variable declaration.
  // Duplicate declarations with the same name/index are allowed (no-op).
  virtual absl::Status DeclareVar(NameToken name, GlobalIndex global_index,
                                  std::size_t length) = 0;
  virtual absl::StatusOr<std::unique_ptr<VarDeclTable>> Build() = 0;
};

// ------------------------------------------------------------------
class VarTable {
 public:
  class Variable {
   public:
    virtual ~Variable() = default;
    virtual NameToken const& token_name() const = 0;
    virtual util::RefStr const& name() const = 0;
    virtual ModuleVarIndex index() const = 0;
    // Unlike in the declaration table, an initial value is always present.
    virtual util::Seq<codegen::LiteralValue const&> initial_value() const = 0;
  };

  virtual ~VarTable() = default;
  virtual util::Seq<Variable const&> vars() const = 0;
  virtual Variable const* LookupByName(std::string_view name) const = 0;
  virtual Variable const* LookupByIndex(ModuleVarIndex var_index) const = 0;
};

class VarTableBuilder {
 public:
  static std::unique_ptr<VarTableBuilder> Create();
  virtual ~VarTableBuilder() = default;

  // Later define the variable with an initial value.
  virtual absl::Status DefineVar(NameToken name, ModuleVarIndex var_index,
                                 std::vector<codegen::LiteralValue> values) = 0;
  virtual absl::StatusOr<std::unique_ptr<VarTable>> Build() = 0;
};

}  // namespace sem

#endif