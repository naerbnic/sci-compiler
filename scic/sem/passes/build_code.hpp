#ifndef SEM_PASSES_BUILD_CODE_HPP
#define SEM_PASSES_BUILD_CODE_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/late_bound.hpp"
#include "scic/sem/selector_table.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/choice.hpp"

namespace sem::passes {

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

  ClassDecl const* LookupByName(std::string_view species) const {
    auto it = name_table_.find(species);
    if (it == name_table_.end()) {
      return nullptr;
    }
    return it->second;
  }

 private:
  std::map<std::size_t, std::unique_ptr<ClassDecl>> table_;
  std::map<std::string_view, ClassDecl const*, std::less<>> name_table_;
};

class ClassDef {
 public:
  struct Property {
    ast::TokenNode<util::RefStr> name;
    SelectorTable::Entry const* selector;
    codegen::LiteralValue value;
  };

  struct Method {
    ast::TokenNode<util::RefStr> name;
    SelectorTable::Entry const* selector;
  };

  ClassDef(ast::TokenNode<util::RefStr> name, ClassDecl const* old_decl,
           std::vector<Property> properties, std::vector<Method> methods)
      : name_(std::move(name)),
        old_decl_(old_decl),
        properties_(std::move(properties)),
        methods_(std::move(methods)) {}

  ast::TokenNode<util::RefStr> const& token_name() const { return name_; }
  util::RefStr const& name() const { return name_.value(); }
  ClassDecl const* old_decl() const { return old_decl_; }
  std::vector<Property> const& properties() const { return properties_; }
  std::vector<Method> const& methods() const { return methods_; }

 private:
  ast::TokenNode<util::RefStr> name_;
  ClassDecl const* old_decl_;
  // If this is non-empty, then this is a redefinition of a previous class.
  LateBound<std::optional<
      util::Choice<ClassDecl const*, ast::TokenNode<util::RefStr>>>>
      super_;
  std::vector<Property> properties_;
  std::vector<Method> methods_;
};

class ClassDefTable {
 public:
  absl::Status AddClassDef(ast::TokenNode<util::RefStr> name,
                           ClassDecl const* class_decl,
                           std::vector<ClassDef::Property> properties,
                           std::vector<ClassDef::Method> methods) {
    if (name_table_.contains(name.value())) {
      return absl::InvalidArgumentError("Class name already exists");
    }
    auto new_decl = std::make_unique<ClassDef>(
        std::move(name), class_decl, std::move(properties), std::move(methods));

    name_table_.emplace(new_decl->name(), new_decl.get());
    pending_classes_.emplace_back(std::move(new_decl));
    return absl::OkStatus();
  }

  ClassDef const* LookupByName(std::string_view name) const {
    auto it = name_table_.find(name);
    if (it == name_table_.end()) {
      return nullptr;
    }
    return it->second;
  }

 private:
  std::vector<std::unique_ptr<ClassDef>> pending_classes_;
  std::map<std::size_t, std::unique_ptr<ClassDef>> classes_;
  std::map<std::string_view, ClassDef const*, std::less<>> name_table_;
};

class ClassTableBuilder {
 public:
  struct Property {
    ast::TokenNode<util::RefStr> name;
    SelectorTable::Entry const* selector;
    // The value of a selector.
    //
    // In a class decl, this must be a number.
    // In a class def, this can be any literal value.
    codegen::LiteralValue value;
  };

  struct Method {
    ast::TokenNode<util::RefStr> name;
    SelectorTable::Entry const* selector;
  };

  absl::Status AddClassDecl(ast::TokenNode<util::RefStr> name,
                            std::size_t species, std::size_t script_num,
                            std::optional<std::size_t> super_num,
                            std::vector<Property> properties,
                            std::vector<Method> methods) {
    for (auto const& prop : properties) {
      if (!prop.value.has<int>()) {
        return absl::InvalidArgumentError(
            "Property value must be a number in a "
            "class declaration.");
      }
    }
    decls_.emplace_back(std::make_unique<ClassDecl>(ClassDecl{
        .name = std::move(name),
        .species = species,
        .script_num = script_num,
        .super_num = super_num,
        .properties = std::move(properties),
        .methods = std::move(methods),
    }));
    return absl::OkStatus();
  }

  absl::Status AddClassDef(ast::TokenNode<util::RefStr> name,
                           std::size_t script_num,
                           std::optional<util::RefStr> super,
                           std::vector<Property> properties,
                           std::vector<Method> methods) {
    defs_.emplace_back(std::make_unique<ClassDef>(ClassDef{
        .name = std::move(name),
        .script_num = script_num,
        .super = super,
        .properties = std::move(properties),
        .methods = std::move(methods),
    }));
    return absl::OkStatus();
  }

 private:
  struct ClassDecl {
    ast::TokenNode<util::RefStr> name;
    std::size_t species;
    std::size_t script_num;
    std::optional<std::size_t> super_num;

    std::vector<Property> properties;
    std::vector<Method> methods;
  };

  struct ClassDef {
    ast::TokenNode<util::RefStr> name;
    std::size_t script_num;
    std::optional<util::RefStr> super;
    std::vector<Property> properties;
    std::vector<Method> methods;
  };

  std::vector<std::unique_ptr<ClassDef>> defs_;
  std::vector<std::unique_ptr<ClassDecl>> decls_;
};

// Gets the script ID from the module with the given items.
absl::StatusOr<std::size_t> GetScriptId(Items items);

// Collects all the selectors that are defined in a module, but have not
// previously been declared.
absl::StatusOr<std::vector<ast::TokenNode<util::RefStr>>> GatherNewSelectors(
    SelectorTable const* sel_table, Items items);

// Collects all the class definitions from a module.
absl::StatusOr<ClassDeclTable> BuildClassDeclGraph(
    SelectorTable const* sel_table, absl::Span<ast::Item const> items);
};  // namespace sem::passes
#endif