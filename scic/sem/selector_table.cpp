#include "scic/sem/selector_table.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
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

absl::Status SelectorTable::InstallSelector(ast::TokenNode<util::RefStr> name,
                                            std::size_t selector_num) {
  if (table_.contains(selector_num)) {
    return absl::InvalidArgumentError("Selector number already exists");
  }

  if (name_map_.contains(name.value())) {
    return absl::InvalidArgumentError("Selector name already exists");
  }

  auto entry = std::make_unique<SelectorTable::Entry>(std::move(name));
  entry->selector_num_.set(selector_num);
  auto const* entry_ptr = entry.get();
  table_.emplace(selector_num, std::move(entry));
  name_map_.emplace(entry_ptr->name(), entry_ptr);
  return absl::OkStatus();
}

absl::Status SelectorTable::AddNewSelector(ast::TokenNode<util::RefStr> name) {
  if (name_map_.contains(name.value())) {
    // This is fine, as we can reuse the entry.
    return absl::OkStatus();
  }
  auto entry = std::make_unique<SelectorTable::Entry>(std::move(name));
  name_map_.emplace(entry->name(), entry.get());
  new_selectors_.emplace_back(std::move(entry));
  return absl::OkStatus();
}

absl::Status SelectorTable::ResolveNewSelectors() {
  // Starting from selector 0, see if there is a gap in the table. If so,
  // assign the next selector number to the new selector.
  // This is not the most efficient method, but it is simple.
  std::size_t next_selector = 0;
  for (auto& entry : new_selectors_) {
    while (table_.contains(next_selector)) {
      ++next_selector;
      if (next_selector > std::numeric_limits<std::uint16_t>::max()) {
        return absl::InvalidArgumentError("Too many selectors");
      }
    }
    entry->selector_num_.set(next_selector++);
    table_.emplace(entry->selector_num(), std::move(entry));
  }
  return absl::OkStatus();
}

SelectorTable::Entry const* SelectorTable::LookupByNumber(
    std::size_t selector_num) const {
  auto it = table_.find(selector_num);
  if (it == table_.end()) {
    return nullptr;
  }
  return it->second.get();
}

SelectorTable::Entry const* SelectorTable::LookupByName(
    std::string_view name) const {
  auto it = name_map_.find(name);
  if (it == name_map_.end()) {
    return nullptr;
  }
  return it->second;
}
}  // namespace sem