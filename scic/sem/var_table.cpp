#include "scic/sem/var_table.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/sem/common.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/sequence.hpp"

namespace sem {
namespace {

class VariableImpl : public VarTable::Variable {
 public:
  VariableImpl(NameToken name, std::size_t global_index, std::size_t length,
               std::optional<std::vector<codegen::LiteralValue>> initial_value =
                   std::nullopt)
      : name_(std::move(name)),
        var_index_(global_index),
        length_(length),
        initial_value_(std::move(initial_value)) {}

  NameToken const& token_name() const override { return name_; }
  util::RefStr const& name() const override { return name_.value(); }
  std::size_t index() const override { return var_index_; }
  std::size_t length() const override { return length_; }
  std::optional<util::Seq<codegen::LiteralValue>> initial_value()
      const override {
    return initial_value_;
  }

  absl::Status SetInitialValue(
      std::vector<codegen::LiteralValue> initial_value) {
    if (initial_value_.has_value()) {
      return absl::InvalidArgumentError("Initial value already set");
    }

    if (initial_value.size() != length_) {
      return absl::InvalidArgumentError("Initial value has wrong length");
    }

    initial_value_ = std::move(initial_value);
    return absl::OkStatus();
  }

 private:
  friend class GlobalTableImpl;

  NameToken name_;
  std::size_t var_index_;
  std::size_t length_;
  std::optional<std::vector<codegen::LiteralValue>> initial_value_;
};

class GlobalTableImpl : public VarTable {
 public:
  GlobalTableImpl(
      std::vector<std::unique_ptr<VariableImpl>> entries,
      std::map<std::size_t, VariableImpl const*, std::less<>> index_table,
      std::map<std::string_view, VariableImpl const*, std::less<>> name_map)
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
  std::vector<std::unique_ptr<VariableImpl>> entries_;
  std::map<std::size_t, VariableImpl const*, std::less<>> index_table_;
  std::map<std::string_view, VariableImpl const*, std::less<>> name_map_;
};

class GlobalTableBuilderImpl : public VarTableBuilder {
 public:
  absl::Status DeclareVar(NameToken name, std::size_t var_index,
                          std::size_t length) override {
    auto name_it = name_map_.find(name.value());
    auto index_it = index_table_.find(var_index);

    auto found_name = name_it != name_map_.end();
    auto found_index = index_it != index_table_.end();

    if (found_name && found_index && name_it->second == index_it->second &&
        name_it->second->length() == length) {
      return absl::OkStatus();
    }
    if (found_name) {
      return absl::InvalidArgumentError("Global name already exists");
    }
    if (found_index) {
      return absl::InvalidArgumentError("Global index already exists");
    }
    auto entry =
        std::make_unique<VariableImpl>(std::move(name), var_index, length);
    index_table_.emplace(var_index, entry.get());
    name_map_.emplace(entry->name(), entry.get());
    entries_.emplace_back(std::move(entry));
    return absl::OkStatus();
  }

  absl::Status DefineVar(
      NameToken name, std::size_t var_index,
      std::vector<codegen::LiteralValue> initial_value) override {
    auto name_it = name_map_.find(name.value());
    auto index_it = index_table_.find(var_index);

    auto found_name = name_it != name_map_.end();
    auto found_index = index_it != index_table_.end();

    if (found_name && found_index && name_it->second == index_it->second) {
      return const_cast<VariableImpl*>(name_it->second)
          ->SetInitialValue(std::move(initial_value));
    }
    if (found_name) {
      return absl::InvalidArgumentError("Global name already exists");
    }
    if (found_index) {
      return absl::InvalidArgumentError("Global index already exists");
    }
    auto entry = std::make_unique<VariableImpl>(std::move(name), var_index,
                                                initial_value.size(),
                                                std::move(initial_value));
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
  std::vector<std::unique_ptr<VariableImpl>> entries_;
  std::map<std::size_t, VariableImpl const*, std::less<>> index_table_;
  std::map<std::string_view, VariableImpl const*, std::less<>> name_map_;
};

}  // namespace

std::unique_ptr<VarTableBuilder> VarTableBuilder::Create() {
  return std::make_unique<GlobalTableBuilderImpl>();
}

}  // namespace sem