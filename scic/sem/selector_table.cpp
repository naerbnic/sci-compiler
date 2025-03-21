#include "scic/sem/selector_table.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "scic/parsers/sci/ast.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/late_bound.hpp"
#include "scic/status/status.hpp"
#include "scic/text/text_range.hpp"
#include "scic/tokens/token_source.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/sequence.hpp"

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
      std::vector<std::unique_ptr<EntryImpl>> entries,
      std::map<SelectorNum, EntryImpl const*> table,
      std::map<std::string_view, EntryImpl const*, std::less<>> name_map)
      : entries_(std::move(entries)),
        table_(std::move(table)),
        name_map_(std::move(name_map)) {}

  util::Seq<Entry const&> entries() const override {
    return util::Seq<Entry const&>::Deref(entries_);
  }

  Entry const* LookupByNumber(SelectorNum selector_num) const override {
    auto it = table_.find(selector_num);
    if (it == table_.end()) {
      return nullptr;
    }
    return it->second;
  }

  Entry const* LookupByName(std::string_view name) const override {
    auto it = name_map_.find(name);
    if (it == name_map_.end()) {
      return nullptr;
    }
    return it->second;
  }

 private:
  std::vector<std::unique_ptr<EntryImpl>> entries_;
  // Map from selector numbers to selector entries. Owns the entries.
  std::map<SelectorNum, EntryImpl const*> table_;
  std::map<std::string_view, EntryImpl const*, std::less<>> name_map_;
};

class BuilderImpl : public SelectorTable::Builder {
 public:
  status::Status DeclareSelector(ast::TokenNode<util::RefStr> name,
                                 SelectorNum selector_num) override {
    auto num_it = num_map_.find(selector_num);
    auto name_it = name_map_.find(name.value());

    auto num_exists = num_it != num_map_.end();
    auto name_exists = name_it != name_map_.end();

    if (num_exists || name_exists) {
      if (num_exists && name_exists) {
        if (num_it->second == name_it->second) {
          // An entry already exists with this combination of name and number.
          return status::OkStatus();
        } else {
          return status::InvalidArgumentError(
              "Selector number and name already exist");
        }
      } else if (num_exists) {
        return status::InvalidArgumentError("Selector number already exists");
      } else {
        return status::InvalidArgumentError("Selector name already exists");
      }
    }

    auto entry = std::make_unique<EntryImpl>(std::move(name));
    entry->selector_num_.set(selector_num);
    name_map_.emplace(entry->name(), entry.get());
    num_map_.emplace(selector_num, entry.get());
    entries_.push_back(std::move(entry));
    return status::OkStatus();
  }

  status::Status AddNewSelector(ast::TokenNode<util::RefStr> name) override {
    if (name_map_.contains(name.value())) {
      // This is fine, as we can reuse the entry.
      return status::OkStatus();
    }
    auto entry = std::make_unique<EntryImpl>(std::move(name));
    name_map_.emplace(entry->name(), entry.get());
    new_selectors_.emplace_back(entry.get());
    entries_.emplace_back(std::move(entry));
    return status::OkStatus();
  }

  status::StatusOr<std::unique_ptr<SelectorTable>> Build() override {
    // Starting from selector 0, see if there is a gap in the table. If so,
    // assign the next selector number to the new selector.
    // This is not the most efficient method, but it is simple.
    std::size_t next_selector = 0;
    for (auto& entry : new_selectors_) {
      while (num_map_.contains(SelectorNum::Create(next_selector))) {
        ++next_selector;
        if (next_selector > std::numeric_limits<std::uint16_t>::max()) {
          return status::InvalidArgumentError("Too many selectors");
        }
      }
      entry->selector_num_.set(SelectorNum::Create(next_selector++));

      num_map_.emplace(entry->selector_num(), entry);
    }

    auto new_table = std::make_unique<SelectorTableImpl>(
        std::move(entries_), std::move(num_map_), std::move(name_map_));

    new_selectors_.clear();
    num_map_.clear();
    name_map_.clear();
    entries_.clear();

    return new_table;
  }

 private:
  std::vector<std::unique_ptr<EntryImpl>> entries_;
  std::map<SelectorNum, EntryImpl const*> num_map_;
  std::vector<EntryImpl*> new_selectors_;
  std::map<std::string_view, EntryImpl const*, std::less<>> name_map_;
};

}  // namespace

std::unique_ptr<SelectorTable::Builder> SelectorTable::CreateBuilder() {
  auto builder = std::make_unique<BuilderImpl>();

  // Check to see if the standard properties are already defined. If not, add
  // them as new selectors.

  std::vector<std::pair<std::string_view, StandardSelectorIndexes>>
      standard_properties = {
          {kObjIdSelName, SEL_OBJID},
          {kSizeSelName, SEL_SIZE},
          {kPropDictSelName, SEL_PROPDICT},
          {kMethDictSelName, SEL_METHDICT},
          {kClassScriptSelName, SEL_CLASS_SCRIPT},
          {kScriptSelName, SEL_SCRIPT},
          {kSuperSelName, SEL_SUPER},
          {kInfoSelName, SEL_INFO},
      };

  for (auto const& [name, selector_num] : standard_properties) {
    auto status = builder->DeclareSelector(
        ast::TokenNode<util::RefStr>(
            util::RefStr(name),
            tokens::TokenSource(text::TextRange::OfString(std::string(name)))),
        SelectorNum::Create(selector_num));
    if (!status.ok()) {
      throw std::runtime_error(absl::StrFormat(
          "Builtin selector shouldn't already exist: %v", status));
    }
  }

  return builder;
}

}  // namespace sem