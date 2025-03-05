#include "scic/sem/var_table.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "scic/codegen/code_generator.hpp"
#include "scic/sem/common.hpp"
#include "scic/status/status.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/sequence.hpp"

namespace sem {
namespace {

// --------------------------
class VariableImpl : public VarTable::Variable {
 public:
  // For a fully defined variable, the initial value must be provided.
  VariableImpl(NameToken name, ModuleVarIndex global_index,
               std::vector<codegen::LiteralValue> initial_value)
      : name_(std::move(name)),
        var_index_(global_index),
        initial_value_(std::move(initial_value)) {}

  NameToken const& token_name() const override { return name_; }
  util::RefStr const& name() const override { return name_.value(); }
  ModuleVarIndex index() const override { return var_index_; }
  util::Seq<codegen::LiteralValue const&> initial_value() const override {
    return initial_value_;
  }

 private:
  NameToken name_;
  ModuleVarIndex var_index_;
  std::vector<codegen::LiteralValue> initial_value_;
};

class VarTableImpl : public VarTable {
 public:
  VarTableImpl(
      std::vector<std::unique_ptr<VariableImpl>> entries,
      std::map<ModuleVarIndex, VariableImpl const*, std::less<>> index_table,
      std::map<std::string_view, VariableImpl const*, std::less<>> name_map)
      : entries_(std::move(entries)),
        index_table_(std::move(index_table)),
        name_map_(std::move(name_map)) {}

  util::Seq<VarTable::Variable const&> vars() const override {
    return util::Seq<VarTable::Variable const&>::Deref(entries_);
  }

  VarTable::Variable const* LookupByName(std::string_view name) const override {
    auto it = name_map_.find(name);
    return it != name_map_.end() ? it->second : nullptr;
  }

  VarTable::Variable const* LookupByIndex(ModuleVarIndex index) const override {
    auto it = index_table_.find(index);
    return it != index_table_.end() ? it->second : nullptr;
  }

 private:
  std::vector<std::unique_ptr<VariableImpl>> entries_;
  std::map<ModuleVarIndex, VariableImpl const*, std::less<>> index_table_;
  std::map<std::string_view, VariableImpl const*, std::less<>> name_map_;
};

class VarTableBuilderImpl : public VarTableBuilder {
 public:
  status::Status DefineVar(
      NameToken name, ModuleVarIndex var_index,
      std::vector<codegen::LiteralValue> initial_value) override {
    if (index_table_.find(var_index) != index_table_.end()) {
      return status::InvalidArgumentError("Variable already defined");
    }
    // Create a new VariableImpl.
    auto entry = std::make_unique<VariableImpl>(std::move(name), var_index,
                                                std::move(initial_value));
    // Update lookup tables.
    index_table_.emplace(var_index, entry.get());
    name_map_.emplace(entry->name(), entry.get());
    entries_.emplace_back(std::move(entry));
    return status::OkStatus();
  }

  status::StatusOr<std::unique_ptr<VarTable>> Build() override {
    return std::make_unique<VarTableImpl>(
        std::move(entries_), std::move(index_table_), std::move(name_map_));
  }

 private:
  std::vector<std::unique_ptr<VariableImpl>> entries_;
  std::map<ModuleVarIndex, VariableImpl const*, std::less<>> index_table_;
  std::map<std::string_view, VariableImpl const*, std::less<>> name_map_;
};

// --------------------------

class DeclVariableImpl : public VarDeclTable::Variable {
 public:
  DeclVariableImpl(NameToken name, GlobalIndex global_index, std::size_t length)
      : name_(std::move(name)), var_index_(global_index), length_(length) {}

  NameToken const& token_name() const override { return name_; }
  util::RefStr const& name() const override { return name_.value(); }
  GlobalIndex index() const override { return var_index_; }
  std::size_t length() const override { return length_; }

 private:
  NameToken name_;
  GlobalIndex var_index_;
  std::size_t length_;
};

class GlobalDeclTableImpl : public VarDeclTable {
 public:
  GlobalDeclTableImpl(
      std::vector<std::unique_ptr<DeclVariableImpl>> entries,
      std::map<GlobalIndex, DeclVariableImpl const*, std::less<>> index_table,
      std::map<std::string_view, DeclVariableImpl const*, std::less<>> name_map)
      : entries_(std::move(entries)),
        index_table_(std::move(index_table)),
        name_table_(std::move(name_map)) {}

  util::Seq<VarDeclTable::Variable const&> vars() const override {
    return util::Seq<VarDeclTable::Variable const&>::Deref(entries_);
  }

  VarDeclTable::Variable const* LookupByName(
      std::string_view name) const override {
    auto it = name_table_.find(name);
    return it != name_table_.end() ? it->second : nullptr;
  }

  VarDeclTable::Variable const* LookupByIndex(
      GlobalIndex index) const override {
    auto it = index_table_.find(index);
    return it != index_table_.end() ? it->second : nullptr;
  }

 private:
  std::vector<std::unique_ptr<DeclVariableImpl>> entries_;
  std::map<GlobalIndex, DeclVariableImpl const*, std::less<>> index_table_;
  std::map<std::string_view, DeclVariableImpl const*, std::less<>> name_table_;
};

class VarDeclTableBuilderImpl : public VarDeclTableBuilder {
 public:
  status::Status DeclareVar(NameToken name, GlobalIndex var_index,
                            std::size_t length) override {
    auto name_it = name_table_.find(name.value());
    auto index_it = index_table_.find(var_index);

    bool found_name = name_it != name_table_.end();
    bool found_index = index_it != index_table_.end();
    if (found_name && found_index && name_it->second == index_it->second &&
        name_it->second->length() == length) {
      return status::OkStatus();
    }
    if (found_name || found_index) {
      return status::InvalidArgumentError("Variable already declared");
    }

    auto entry =
        std::make_unique<DeclVariableImpl>(std::move(name), var_index, length);
    index_table_.emplace(var_index, entry.get());
    name_table_.emplace(entry->name(), entry.get());
    entries_.emplace_back(std::move(entry));
    return status::OkStatus();
  }

  status::StatusOr<std::unique_ptr<VarDeclTable>> Build() override {
    return std::make_unique<GlobalDeclTableImpl>(
        std::move(entries_), std::move(index_table_), std::move(name_table_));
  }

 private:
  std::vector<std::unique_ptr<DeclVariableImpl>> entries_;
  std::map<GlobalIndex, DeclVariableImpl const*, std::less<>> index_table_;
  std::map<std::string_view, DeclVariableImpl const*, std::less<>> name_table_;
};

}  // namespace

// Expose builders.
std::unique_ptr<VarTableBuilder> VarTableBuilder::Create() {
  return std::make_unique<VarTableBuilderImpl>();
}

std::unique_ptr<VarDeclTableBuilder> VarDeclTableBuilder::Create() {
  return std::make_unique<VarDeclTableBuilderImpl>();
}

}  // namespace sem