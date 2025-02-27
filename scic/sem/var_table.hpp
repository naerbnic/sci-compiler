#ifndef SEM_GLOBAL_TABLE_HPP
#define SEM_GLOBAL_TABLE_HPP

#include <cstddef>
#include <memory>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
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
  };

  virtual ~VarTable() = default;

  virtual util::Seq<Variable const&> vars() const = 0;

  virtual Variable const* LookupByName(std::string_view name) const = 0;
  virtual Variable const* LookupByIndex(std::size_t global_index) const = 0;
};

class GlobalTableBuilder {
 public:
  static std::unique_ptr<GlobalTableBuilder> Create();

  virtual ~GlobalTableBuilder() = default;

  virtual absl::Status DeclareVar(NameToken name, std::size_t global_index) = 0;
  virtual absl::StatusOr<std::unique_ptr<VarTable>> Build() = 0;
};

}  // namespace sem
#endif