#ifndef SEM_PASSES_BUILD_CODE_HPP
#define SEM_PASSES_BUILD_CODE_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "scic/parsers/sci/ast.hpp"
#include "util/strings/ref_str.hpp"

namespace sem::passes {

namespace ast = parsers::sci;

using Items = absl::Span<ast::Item const>;

// Similar to an Optional, but is intended to only be updated once.
//
// This should be used as a field, as the mutators do not allow for arbitrary
// assignment, which would make it less useful for a parameter.
template <class T>
class LateBound {
 public:
  LateBound() = default;

  void set(T value) {
    if (value_.has_value()) {
      throw std::logic_error("LateBound value already set");
    }
    value_ = std::move(value);
  }

  // Pointer semantics
  T* operator->() { return &value_.value(); }
  T const* operator->() const { return &value_.value(); }
  T& operator*() { return value_.value(); }
  T const& operator*() const { return value_.value(); }

 private:
  std::optional<T> value_;
};

class SelectorTable {
 public:
  class Entry {
   public:
    Entry(ast::TokenNode<util::RefStr> name, std::size_t selector_num)
        : name_(std::move(name)), selector_num_(selector_num) {}

    ast::TokenNode<util::RefStr> const& name_token() const { return name_; }
    util::RefStr const& name() const { return name_.value(); }
    std::size_t selector_num() const { return selector_num_; }

   private:
    ast::TokenNode<util::RefStr> name_;
    std::size_t selector_num_;
  };

  absl::Status InstallSelector(ast::TokenNode<util::RefStr> name,
                               std::size_t selector_num) {
    if (table_.contains(selector_num)) {
      return absl::InvalidArgumentError("Selector number already exists");
    }

    if (name_map_.contains(name.value())) {
      return absl::InvalidArgumentError("Selector name already exists");
    }

    auto entry =
        std::make_unique<SelectorTable::Entry>(std::move(name), selector_num);
    auto const* entry_ptr = entry.get();
    table_.emplace(selector_num, std::move(entry));
    name_map_.emplace(entry_ptr->name(), entry_ptr);
    return absl::OkStatus();
  }

  Entry const* LookupByNumber(std::size_t selector_num) const {
    auto it = table_.find(selector_num);
    if (it == table_.end()) {
      return nullptr;
    }
    return it->second.get();
  }

  Entry const* LookupByName(std::string_view name) const {
    auto it = name_map_.find(name);
    if (it == name_map_.end()) {
      return nullptr;
    }
    return it->second;
  }

 private:
  // Map from selector numbers to selector entries. Owns the entries.
  std::map<std::size_t, std::unique_ptr<Entry>> table_;
  std::map<std::string_view, Entry const*, std::less<>> name_map_;
};

class ClassDecl {
 public:
  struct Property {
    parsers::sci::TokenNode<util::RefStr> name;
    SelectorTable::Entry const* selector;
    std::uint16_t value;
  };

  ClassDecl(ast::TokenNode<util::RefStr> name, std::size_t species,
            std::size_t script_num, std::vector<Property> properties,
            std::vector<SelectorTable::Entry const*> methods)
      : name_(std::move(name)),
        species_(species),
        script_num_(script_num),
        properties_(std::move(properties)),
        methods_(std::move(methods)) {}

  ast::TokenNode<util::RefStr> const& get_name() const { return name_; }
  std::size_t get_species() const { return species_; }
  std::size_t get_script_num() const { return script_num_; }
  ClassDecl const* get_super() const { return *super_; }

  // Late binding setters. These should only be called once during the creation
  // of the class graph.
  void set_super(ClassDecl const* new_super) { super_.set(new_super); }

 private:
  ast::TokenNode<util::RefStr> name_;
  std::size_t species_;
  std::size_t script_num_;
  LateBound<ClassDecl const*> super_;

  std::vector<Property> properties_;
  std::vector<SelectorTable::Entry const*> methods_;
};

class ClassDeclTable {
 public:
  ClassDeclTable() = default;

  absl::Status AddClass(ast::TokenNode<util::RefStr> name, std::size_t species,
                        std::size_t script_num,
                        std::vector<ClassDecl::Property> properties,
                        std::vector<SelectorTable::Entry const*> methods) {
    if (table_.contains(species)) {
      return absl::InvalidArgumentError("Class species already exists");
    }

    if (name_table_.contains(name.value())) {
      return absl::InvalidArgumentError("Class name already exists");
    }

    auto new_decl =
        std::make_unique<ClassDecl>(std::move(name), species, script_num,
                                    std::move(properties), std::move(methods));
    auto const* new_decl_ptr = new_decl.get();

    table_.emplace(species, std::move(new_decl));
    name_table_.emplace(new_decl_ptr->get_name().value(), new_decl_ptr);

    return absl::OkStatus();
  }

  absl::Status SetClassSuper(std::size_t base_class,
                             std::optional<std::size_t> super_num) {
    auto it = table_.find(base_class);
    if (it == table_.end()) {
      return absl::FailedPreconditionError("Base class not found");
    }
    ClassDecl const* super = nullptr;
    if (super_num) {
      auto it = table_.find(*super_num);
      if (it == table_.end()) {
        return absl::InvalidArgumentError("Parent class not found");
      }
      super = it->second.get();
    }
    it->second->set_super(super);
    return absl::OkStatus();
  }

  ClassDecl const* LookupBySpecies(std::size_t species) const {
    auto it = table_.find(species);
    if (it == table_.end()) {
      return nullptr;
    }
    return it->second.get();
  }

  ClassDecl const* LookupByName(std::size_t species) const {
    auto it = table_.find(species);
    if (it == table_.end()) {
      return nullptr;
    }
    return it->second.get();
  }

 private:
  std::map<std::size_t, std::unique_ptr<ClassDecl>> table_;
  std::map<std::string_view, ClassDecl const*, std::less<>> name_table_;
};
absl::StatusOr<std::size_t> GetScriptId(Items items);
absl::StatusOr<SelectorTable> BuildSelectorTable(
    absl::Span<ast::Item const> items);

};  // namespace sem::passes
#endif