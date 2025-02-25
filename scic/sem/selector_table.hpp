#ifndef SEM_SELECTOR_TABLE_HPP
#define SEM_SELECTOR_TABLE_HPP

#include <memory>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "scic/parsers/sci/ast.hpp"
#include "scic/sem/common.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/sequence.hpp"

namespace sem {

// A table of declared selectors.
class SelectorTable {
 public:
  class Builder {
   public:
    virtual ~Builder() = default;
    virtual absl::Status DeclareSelector(ast::TokenNode<util::RefStr> name,
                                         SelectorNum selector_num) = 0;
    virtual absl::Status AddNewSelector(ast::TokenNode<util::RefStr> name) = 0;
    virtual absl::StatusOr<std::unique_ptr<SelectorTable>> Build() = 0;
  };

  static std::unique_ptr<Builder> CreateBuilder();

  class Entry {
   public:
    virtual ~Entry() = default;

    virtual ast::TokenNode<util::RefStr> const& name_token() const = 0;
    virtual util::RefStr const& name() const = 0;
    virtual SelectorNum selector_num() const = 0;
  };

  virtual ~SelectorTable() = default;

  virtual util::Seq<Entry const&> entries() const = 0;

  virtual Entry const* LookupByNumber(SelectorNum selector_num) const = 0;

  virtual Entry const* LookupByName(std::string_view name) const = 0;
};
}  // namespace sem

#endif