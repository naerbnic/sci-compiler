#include "scic/sem/selector_table.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "scic/parsers/sci/ast.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/late_bound.hpp"
#include "util/strings/ref_str.hpp"

namespace sem {
namespace {

class SelectorTableImpl;
class BuilderImpl;

class EntryImpl : public SelectorTable::Entry {
 public:
  EntryImpl(ast::TokenNode<util::RefStr> name) : name_(std::move(name)) {}

  ast::TokenNode<util::RefStr> const& name_token() const override {
    return name_;
  }
  util::RefStr const& name() const override { return name_.value(); }
  SelectorNum selector_num() const override { return *selector_num_; }

 private:
  friend class SelectorTableImpl;
  friend class BuilderImpl;

  ast::TokenNode<util::RefStr> name_;
  LateBound<SelectorNum> selector_num_;
};

// A table of declared selectors.
class SelectorTableImpl : public SelectorTable {
 public:
  SelectorTableImpl(
      std::map<SelectorNum, std::unique_ptr<EntryImpl>> table,
      std::map<std::string_view, EntryImpl const*, std::less<>> name_map)
      : table_(std::move(table)), name_map_(std::move(name_map)) {}

  Entry const* LookupByNumber(SelectorNum selector_num) const override {
    auto it = table_.find(selector_num);
    if (it == table_.end()) {
      return nullptr;
    }
    return it->second.get();
  }

  Entry const* LookupByName(std::string_view name) const override {
    auto it = name_map_.find(name);
    if (it == name_map_.end()) {
      return nullptr;
    }
    return it->second;
  }

 private:
  // Map from selector numbers to selector entries. Owns the entries.
  std::map<SelectorNum, std::unique_ptr<EntryImpl>> table_;
  std::map<std::string_view, EntryImpl const*, std::less<>> name_map_;
};

class BuilderImpl : public SelectorTable::Builder {
 public:
  absl::Status DeclareSelector(ast::TokenNode<util::RefStr> name,
                               SelectorNum selector_num) override {
    if (table_.contains(selector_num)) {
      return absl::InvalidArgumentError("Selector number already exists");
    }

    if (name_map_.contains(name.value())) {
      return absl::InvalidArgumentError("Selector name already exists");
    }

    auto entry = std::make_unique<EntryImpl>(std::move(name));
    entry->selector_num_.set(selector_num);
    auto const* entry_ptr = entry.get();
    table_.emplace(selector_num, std::move(entry));
    name_map_.emplace(entry_ptr->name(), entry_ptr);
    return absl::OkStatus();
  }

  absl::Status AddNewSelector(ast::TokenNode<util::RefStr> name) override {
    if (name_map_.contains(name.value())) {
      // This is fine, as we can reuse the entry.
      return absl::OkStatus();
    }
    auto entry = std::make_unique<EntryImpl>(std::move(name));
    name_map_.emplace(entry->name(), entry.get());
    new_selectors_.emplace_back(std::move(entry));
    return absl::OkStatus();
  }

  absl::StatusOr<std::unique_ptr<SelectorTable>> Build() override {
    // Starting from selector 0, see if there is a gap in the table. If so,
    // assign the next selector number to the new selector.
    // This is not the most efficient method, but it is simple.
    std::size_t next_selector = 0;
    for (auto& entry : new_selectors_) {
      while (table_.contains(SelectorNum::Create(next_selector))) {
        ++next_selector;
        if (next_selector > std::numeric_limits<std::uint16_t>::max()) {
          return absl::InvalidArgumentError("Too many selectors");
        }
      }
      entry->selector_num_.set(SelectorNum::Create(next_selector++));
      table_.emplace(entry->selector_num(), std::move(entry));
    }

    auto new_table = std::make_unique<SelectorTableImpl>(std::move(table_),
                                                         std::move(name_map_));

    new_selectors_.clear();
    table_.clear();
    name_map_.clear();

    return new_table;
  }

 private:
  // Map from selector numbers to selector entries. Owns the entries.
  std::map<SelectorNum, std::unique_ptr<EntryImpl>> table_;
  std::vector<std::unique_ptr<EntryImpl>> new_selectors_;
  std::map<std::string_view, EntryImpl const*, std::less<>> name_map_;
};

}  // namespace

}  // namespace sem