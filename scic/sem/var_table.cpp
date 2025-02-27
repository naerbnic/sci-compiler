#include "scic/sem/var_table.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "scic/sem/common.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/sequence.hpp"

namespace sem {
namespace {

class GlobalImpl : public VarTable::Variable {
 public:
  GlobalImpl(NameToken name, std::size_t global_index)
      : name_(std::move(name)), var_index_(global_index) {}

  NameToken const& token_name() const override { return name_; }
  util::RefStr const& name() const override { return name_.value(); }
  std::size_t index() const override { return var_index_; }

 private:
  friend class GlobalTableImpl;

  NameToken name_;
  std::size_t var_index_;
};

class GlobalTableImpl : public VarTable {
 public:
  GlobalTableImpl(
      std::vector<std::unique_ptr<GlobalImpl>> entries,
      std::map<std::size_t, GlobalImpl const*, std::less<>> index_table,
      std::map<std::string_view, GlobalImpl const*, std::less<>> name_map)
      : entries_(std::move(entries)),
        index_table_(std::move(index_table)),
        name_map_(std::move(name_map)) {}

  util::Seq<Variable const&> vars() const override {
    return util::Seq<Variable const&>::Deref(entries_);
  }

  Variable const* LookupByName(std::string_view name) const override {
    auto it = name_map_.find(name);
    if (it == name_map_.end()) {
      return nullptr;
    }
    return it->second;
  }

  Variable const* LookupByIndex(std::size_t index) const override {
    auto it = index_table_.find(index);
    if (it == index_table_.end()) {
      return nullptr;
    }
    return it->second;
  }

 private:
  std::vector<std::unique_ptr<GlobalImpl>> entries_;
  std::map<std::size_t, GlobalImpl const*, std::less<>> index_table_;
  std::map<std::string_view, GlobalImpl const*, std::less<>> name_map_;
};

class GlobalTableBuilderImpl : public GlobalTableBuilder {
 public:
  absl::Status DeclareVar(NameToken name, std::size_t var_index) override {
    auto name_it = name_map_.find(name.value());
    auto index_it = index_table_.find(var_index);

    auto found_name = name_it != name_map_.end();
    auto found_index = index_it != index_table_.end();

    if (found_name && found_index && name_it->second == index_it->second) {
      return absl::OkStatus();
    }
    if (found_name) {
      return absl::InvalidArgumentError("Global name already exists");
    }
    if (found_index) {
      return absl::InvalidArgumentError("Global index already exists");
    }
    auto entry = std::make_unique<GlobalImpl>(std::move(name), var_index);
    index_table_.emplace(var_index, entry.get());
    name_map_.emplace(entry->name(), entry.get());
    entries_.emplace_back(std::move(entry));
    return absl::OkStatus();
  }

  absl::StatusOr<std::unique_ptr<VarTable>> Build() override {
    return std::make_unique<GlobalTableImpl>(
        std::move(entries_), std::move(index_table_), std::move(name_map_));
  }

 private:
  std::vector<std::unique_ptr<GlobalImpl>> entries_;
  std::map<std::size_t, GlobalImpl const*, std::less<>> index_table_;
  std::map<std::string_view, GlobalImpl const*, std::less<>> name_map_;
};

}  // namespace
}  // namespace sem