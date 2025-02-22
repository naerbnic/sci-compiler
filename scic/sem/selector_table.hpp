#ifndef SEM_SELECTOR_TABLE_HPP
#define SEM_SELECTOR_TABLE_HPP

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "scic/parsers/sci/ast.hpp"
#include "scic/sem/late_bound.hpp"
#include "util/strings/ref_str.hpp"

namespace sem {

namespace ast = parsers::sci;

// A table of declared selectors.
class SelectorTable {
 public:
  class Entry {
   public:
    Entry(ast::TokenNode<util::RefStr> name) : name_(std::move(name)) {}

    ast::TokenNode<util::RefStr> const& name_token() const { return name_; }
    util::RefStr const& name() const { return name_.value(); }
    std::size_t selector_num() const { return *selector_num_; }

   private:
    friend class SelectorTable;

    ast::TokenNode<util::RefStr> name_;
    LateBound<std::size_t> selector_num_;
  };

  absl::Status InstallSelector(ast::TokenNode<util::RefStr> name,
                               std::size_t selector_num);

  absl::Status AddNewSelector(ast::TokenNode<util::RefStr> name);

  absl::Status ResolveNewSelectors();

  Entry const* LookupByNumber(std::size_t selector_num) const;

  Entry const* LookupByName(std::string_view name) const;

 private:
  // Map from selector numbers to selector entries. Owns the entries.
  std::map<std::size_t, std::unique_ptr<Entry>> table_;
  std::vector<std::unique_ptr<Entry>> new_selectors_;
  std::map<std::string_view, Entry const*, std::less<>> name_map_;
};
}  // namespace sem

#endif