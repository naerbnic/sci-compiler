#include "scic/sem/class_table.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/late_bound.hpp"
#include "scic/sem/selector_table.hpp"
#include "util/status/status_macros.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/sequence.hpp"

namespace sem {

namespace {

// A definition of a property in a class. This does not include the index
// of the property in the class, as that is not known until the class is
// fully resolved against its super class (or lack thereof).
struct PropertyDef {
  PropertyDef(NameToken name, SelectorTable::Entry const* selector,
              codegen::LiteralValue value)
      : name(std::move(name)), selector(selector), value(value) {}
  NameToken name;
  SelectorTable::Entry const* selector;
  codegen::LiteralValue value;
};

class PropertyImpl : public Class::Property {
 public:
  PropertyImpl(NameToken name, PropIndex prop_index,
               SelectorTable::Entry const* selector,
               codegen::LiteralValue value)
      : name_(std::move(name)),
        prop_index_(prop_index),
        selector_(selector),
        value_(value) {}

  NameToken const& token_name() const override { return name_; }
  PropIndex index() const override { return prop_index_; }
  util::RefStr const& name() const override { return name_.value(); }
  SelectorTable::Entry const* selector() const override { return selector_; }
  codegen::LiteralValue value() const override { return value_; }

  void SetValue(codegen::LiteralValue value) { value_ = value; }

 private:
  NameToken name_;
  PropIndex prop_index_;
  SelectorTable::Entry const* selector_;
  codegen::LiteralValue value_;
};

// A full property list for a class.
//
// This includes all properties, including those inherited from super classes.
//
// When an object or class is created, all of its properties are laid out in
// memory, and are used both for initialization and for memory storage, so all
// properties must be known at that time.
class PropertyList {
 public:
  PropertyList() = default;
  PropertyList(PropertyList const&) = delete;
  PropertyList(PropertyList&&) = default;
  PropertyList& operator=(PropertyList const&) = delete;
  PropertyList& operator=(PropertyList&&) = default;

  // Adds a property definition to the list. If a property with the same name
  // already exists, it is updated with the new value.
  //
  // Returns the index of the property in the list.
  //
  // The order of evaluation of this method is significant. For each new
  // property, it will be appended to the current list of properties.
  PropIndex UpdatePropertyDef(NameToken name,
                              SelectorTable::Entry const* selector,
                              codegen::LiteralValue value) {
    auto name_it = name_table_.find(name.value());
    if (name_it != name_table_.end()) {
      // The property already exists. Update the value, and return the index.
      name_it->second->SetValue(value);
      return name_it->second->index();
    }

    auto new_index = PropIndex::Create(properties_.size());
    auto new_prop = std::make_unique<PropertyImpl>(std::move(name), new_index,
                                                   selector, std::move(value));
    AddToIndex(new_prop.get());
    properties_.push_back(std::move(new_prop));
    return new_index;
  }

  PropIndex UpdatePropertyDef(SelectorTable::Entry const* selector,
                              codegen::LiteralValue value) {
    return UpdatePropertyDef(selector->name_token(), selector,
                             std::move(value));
  }

  PropertyList Clone() const {
    std::vector<std::unique_ptr<PropertyImpl>> new_properties;
    for (auto& prop : properties_) {
      new_properties.push_back(std::make_unique<PropertyImpl>(*prop));
    }
    return PropertyList(std::move(new_properties));
  }

  util::Seq<Class::Property const&> properties() const {
    return util::Seq<Class::Property const&>::Deref(properties_);
  }
  std::size_t size() const { return properties_.size(); }

 private:
  PropertyList(std::vector<std::unique_ptr<PropertyImpl>> properties)
      : properties_(std::move(properties)) {
    for (auto& prop : properties_) {
      AddToIndex(prop.get());
    }
  }

  void AddToIndex(PropertyImpl* prop) {
    name_table_.emplace(prop->name(), prop);
    selector_table_.emplace(prop->selector(), prop);
    index_table_.emplace(prop->index(), prop);
  }

  std::vector<std::unique_ptr<PropertyImpl>> properties_;
  std::map<std::string_view, PropertyImpl*, std::less<>> name_table_;
  std::map<SelectorTable::Entry const*, PropertyImpl*, std::less<>>
      selector_table_;
  std::map<PropIndex, PropertyImpl*, std::less<>> index_table_;
};

class MethodImpl : public Class::Method {
 public:
  MethodImpl(NameToken name, SelectorTable::Entry const* selector)
      : name_(std::move(name)), selector_(selector) {}

  NameToken const& token_name() const override { return name_; }
  util::RefStr const& name() const override { return name_.value(); }
  SelectorTable::Entry const* selector() const override { return selector_; }

 private:
  NameToken name_;
  SelectorTable::Entry const* selector_;
};

class ClassImpl : public Class {
 public:
  ClassImpl(NameToken name, ScriptNum script_num, ClassSpecies species,
            absl::Nullable<Class const*> prev_decl,
            std::vector<PropertyDef> property_defs,
            std::vector<MethodImpl> methods)
      : name_(std::move(name)),
        script_num_(script_num),
        species_(species),
        prev_decl_(prev_decl),
        property_defs_(std::move(property_defs)),
        methods_(std::move(methods)) {}

  NameToken const& token_name() const override { return name_; }
  util::RefStr const& name() const override { return name_.value(); }
  ScriptNum script_num() const override { return script_num_; }
  ClassSpecies species() const override { return species_; }
  absl::Nullable<Class const*> super() const override { return *super_; }
  absl::Nullable<Class const*> prev_decl() const override { return prev_decl_; }

  std::size_t prop_size() const override { return property_list_->size(); }

  util::Seq<Property const&> properties() const override {
    return property_list_->properties();
  }

  util::Seq<Method const&> methods() const override { return methods_; }

  void SetSuper(ClassImpl* new_super) { super_.set(new_super); }

  absl::Status ResolveProperties(SelectorTable const* selector_table) {
    if (property_list_.has_value()) {
      // We already have resolved this object. Nothing to be done.
      return absl::OkStatus();
    }
    auto* super = *super_;

    PropertyList property_list;

    // Resolve super classes first.
    if (super) {
      RETURN_IF_ERROR(super->ResolveProperties(selector_table));

      property_list = super->property_list_->Clone();
    } else {
      // Initialize properties with the standard properties.
      property_list.UpdatePropertyDef(
          selector_table->LookupByName(kObjIdSelName), 0x1234);
      property_list.UpdatePropertyDef(
          selector_table->LookupByName(kSizeSelName), 0);
      property_list.UpdatePropertyDef(
          selector_table->LookupByName(kPropDictSelName), 0);
      property_list.UpdatePropertyDef(
          selector_table->LookupByName(kMethDictSelName), 0);
      property_list.UpdatePropertyDef(
          selector_table->LookupByName(kClassScriptSelName), 0);
      property_list.UpdatePropertyDef(
          selector_table->LookupByName(kScriptSelName),
          int(script_num_.value()));
      property_list.UpdatePropertyDef(
          selector_table->LookupByName(kSuperSelName),
          int(*super_ ? super->species_.value() : 0xFFFF));
      // This is a class, so the info has the class bit set.
      property_list.UpdatePropertyDef(
          selector_table->LookupByName(kInfoSelName), 0x8000);
    }

    std::optional<PropIndex> last_index;

    for (auto& prop : property_defs_) {
      auto prop_index =
          property_list.UpdatePropertyDef(prop.name, prop.selector, prop.value);
      if (last_index) {
        // FIXME: We should probably check that the order of the properties
        // is the same as the order in the class definition.
      }
      last_index = prop_index;
    }

    // Now that we have all of the properties, we can set the size of the
    // class.
    property_list.UpdatePropertyDef(selector_table->LookupByName(kSizeSelName),
                                    int(property_list.size()));

    property_list_.set(std::move(property_list));
    return absl::OkStatus();
  }

 private:
  NameToken name_;
  ScriptNum script_num_;
  ClassSpecies species_;
  LateBound<absl::Nullable<ClassImpl*>> super_;
  absl::Nullable<Class const*> prev_decl_;
  std::vector<PropertyDef> property_defs_;
  LateBound<PropertyList> property_list_;
  std::vector<MethodImpl> methods_;
};

// A single layer of class definitions. All references in the class objects
// should be to this layer, with the exception of the prev_decl field.
class ClassTableLayer {
 public:
  absl::Status AddClass(NameToken name, ScriptNum script_num,
                        ClassSpecies species,
                        absl::Nullable<Class const*> prev_decl,
                        std::vector<PropertyDef> property_defs,
                        std::vector<MethodImpl> methods) {
    if (name_table_.contains(name.value())) {
      return absl::InvalidArgumentError("Class name already exists");
    }

    if (species_table_.contains(species)) {
      return absl::InvalidArgumentError("Class species already exists");
    }

    auto new_class = std::make_unique<ClassImpl>(
        std::move(name), script_num, species, prev_decl,
        std::move(property_defs), std::move(methods));
    auto* new_class_ptr = new_class.get();
    classes_.push_back(std::move(new_class));
    name_table_.emplace(new_class_ptr->name(), new_class_ptr);
    species_table_.emplace(new_class_ptr->species(), new_class_ptr);
    return absl::OkStatus();
  }

  absl::Status SetClassSuper(ClassSpecies species,
                             std::optional<ClassSpecies> super_species) {
    auto it = species_table_.find(species);
    if (it == species_table_.end()) {
      return absl::InvalidArgumentError("Class species not found");
    }
    auto* class_impl = it->second;

    if (super_species.has_value()) {
      auto it2 = species_table_.find(*super_species);
      if (it2 == species_table_.end()) {
        return absl::InvalidArgumentError("Class species not found");
      }
      class_impl->SetSuper(it2->second);
    } else {
      class_impl->SetSuper(nullptr);
    }
    return absl::OkStatus();
  }

  absl::Status ResolveProperties(SelectorTable const* selector_table) {
    for (auto& class_impl : classes_) {
      RETURN_IF_ERROR(class_impl->ResolveProperties(selector_table));
    }
    return absl::OkStatus();
  }

  util::Seq<Class const&> classes() const {
    return util::Seq<Class const&>::Deref(classes_);
  }

  Class const* LookupBySpecies(ClassSpecies species) const {
    auto it = species_table_.find(species);
    if (it == species_table_.end()) {
      return nullptr;
    }
    return it->second;
  }

  Class const* LookupByName(std::string_view name) const {
    auto it = name_table_.find(name);
    if (it == name_table_.end()) {
      return nullptr;
    }
    return it->second;
  }

 private:
  std::vector<std::unique_ptr<ClassImpl>> classes_;

  std::map<std::string_view, ClassImpl*, std::less<>> name_table_;
  std::map<ClassSpecies, ClassImpl*, std::less<>> species_table_;
};

class ClassTableImpl : public ClassTable {
 public:
  explicit ClassTableImpl(ClassTableLayer decl_layer, ClassTableLayer def_layer)
      : decl_layer_(std::move(decl_layer)), def_layer_(std::move(def_layer)) {}

  util::Seq<Class const&> classes() const override {
    return def_layer_.classes();
  }

  absl::Nullable<Class const*> LookupBySpecies(
      ClassSpecies species) const override {
    return def_layer_.LookupBySpecies(species);
  }

  absl::Nullable<Class const*> LookupByName(
      std::string_view name) const override {
    return def_layer_.LookupByName(name);
  }

  util::Seq<Class const&> decl_classes() const override {
    return decl_layer_.classes();
  }

  absl::Nullable<Class const*> LookupDeclBySpecies(
      ClassSpecies species) const override {
    return decl_layer_.LookupBySpecies(species);
  }

  absl::Nullable<Class const*> LookupDeclByName(
      std::string_view name) const override {
    return decl_layer_.LookupByName(name);
  }

 private:
  ClassTableLayer decl_layer_;
  ClassTableLayer def_layer_;
};

class ClassTableBuilderImpl : public ClassTableBuilder {
 public:
  explicit ClassTableBuilderImpl(SelectorTable const* sel_table)
      : sel_table_(sel_table) {}

  absl::Status AddClassDecl(NameToken name, ScriptNum script_num,
                            ClassSpecies species,
                            std::optional<ClassSpecies> super_species,
                            std::vector<Property> properties,
                            std::vector<NameToken> methods) override {
    for (auto const& prop : properties) {
      if (!prop.value.has<int>()) {
        return absl::InvalidArgumentError(
            "Property value must be a number in a "
            "class declaration.");
      }
    }
    decls_.push_back(ClassDecl{
        .base =
            ClassBase{
                .name = std::move(name),
                .script_num = script_num,
                .properties = std::move(properties),
                .methods = std::move(methods),
            },
        .species = species,
        .super_num = super_species,
    });
    return absl::OkStatus();
  }

  absl::Status AddClassDef(NameToken name, ScriptNum script_num,
                           std::optional<NameToken> super_name,
                           std::vector<Property> properties,
                           std::vector<NameToken> methods) override {
    defs_.push_back(ClassDef{
        .base =
            ClassBase{
                .name = std::move(name),
                .script_num = script_num,
                .properties = std::move(properties),
                .methods = std::move(methods),
            },
        .super_name = super_name,
    });
    return absl::OkStatus();
  }

  absl::StatusOr<std::unique_ptr<ClassTable>> Build() override {
    // Start by building the declaration layer.
    ClassTableLayer decl_layer;
    {
      for (auto const& decl : decls_) {
        RETURN_IF_ERROR(WriteDeclToLayer(decl_layer, decl, nullptr));
      }

      // Now patch in the super classes.
      for (auto const& decl : decls_) {
        std::optional<ClassSpecies> super_species;
        if (decl.super_num.has_value()) {
          super_species = *decl.super_num;
        }
        RETURN_IF_ERROR(decl_layer.SetClassSuper(decl.species, super_species));
      }

      RETURN_IF_ERROR(decl_layer.ResolveProperties(sel_table_));
    }
    // Now build the definition layer.
    ClassTableLayer def_layer;
    {
      std::size_t curr_species = 0;
      auto find_next_species = [&]() {
        while (def_layer.LookupBySpecies(ClassSpecies::Create(curr_species))) {
          ++curr_species;
        }
        auto species = ClassSpecies::Create(curr_species);
        ++curr_species;
        return species;
      };

      // Keeps track of which species are new in the definition layer.
      std::set<ClassSpecies> def_species;

      // Go through all of the defs, adding them to the layer.
      for (auto const& def : defs_) {
        auto const& name = def.base.name;
        auto const* prev_decl = decl_layer.LookupByName(name.value());

        ClassSpecies species =
            prev_decl ? prev_decl->species() : find_next_species();
        RETURN_IF_ERROR(
            WriteBaseToLayer(def_layer, def.base, species, prev_decl));
        def_species.insert(species);
      }

      // Now go through all of the current declarations, and add them to the
      // layer if they haven't been defined already.
      for (auto const& decl : decls_) {
        if (def_species.contains(decl.species)) {
          continue;
        }
        auto const& name = decl.base.name;
        auto const* prev_decl = decl_layer.LookupByName(name.value());

        RETURN_IF_ERROR(WriteDeclToLayer(def_layer, decl, prev_decl));
      }

      // Now patch in the super classes from the defs.
      for (auto const& def : defs_) {
        auto const* curr_decl = def_layer.LookupByName(def.base.name.value());
        std::optional<ClassSpecies> super_species;
        if (def.super_name.has_value()) {
          // We have to look up our super class by name, as that's how
          // definitions specify the super class.
          auto const* super_decl =
              def_layer.LookupByName(def.super_name->value());
          if (!super_decl) {
            return absl::InvalidArgumentError("Super class not found");
          }
          super_species = super_decl->species();
        }
        RETURN_IF_ERROR(
            def_layer.SetClassSuper(curr_decl->species(), super_species));
      }

      //... and now the decls.
      for (auto const& decl : decls_) {
        if (def_species.contains(decl.species)) {
          continue;
        }

        auto const* curr_decl = def_layer.LookupByName(decl.base.name.value());
        RETURN_IF_ERROR(
            def_layer.SetClassSuper(curr_decl->species(), decl.super_num));
      }

      RETURN_IF_ERROR(def_layer.ResolveProperties(sel_table_));
    }

    return std::make_unique<ClassTableImpl>(std::move(decl_layer),
                                            std::move(def_layer));
  }

 private:
  // The base of both class declarations and definitions.
  struct ClassBase {
    ast::TokenNode<util::RefStr> name;
    ScriptNum script_num;

    std::vector<Property> properties;
    std::vector<NameToken> methods;
  };

  struct ClassDecl {
    ClassBase base;
    ClassSpecies species;
    std::optional<ClassSpecies> super_num;
  };

  struct ClassDef {
    ClassBase base;
    std::optional<NameToken> super_name;
  };

  absl::Status WriteBaseToLayer(ClassTableLayer& layer, ClassBase const& base,
                                ClassSpecies species,
                                absl::Nullable<Class const*> prev_decl) {
    auto const& name = base.name;
    std::vector<PropertyDef> properties;
    for (auto const& prop : base.properties) {
      auto const* selector = sel_table_->LookupByName(prop.name.value());
      if (!selector) {
        return absl::InvalidArgumentError("Selector not found");
      }

      ASSIGN_OR_RETURN(auto value, ConvertToMachineWord(prop.value.as<int>()));
      properties.push_back(PropertyDef(prop.name, selector, value));
    }

    std::vector<MethodImpl> methods;
    for (auto const& method_name : base.methods) {
      auto const* selector = sel_table_->LookupByName(method_name.value());
      if (!selector) {
        return absl::InvalidArgumentError("Selector not found");
      }
      methods.push_back(MethodImpl(method_name, selector));
    }

    return layer.AddClass(name, base.script_num, species, prev_decl,
                          std::move(properties), methods);
  }

  absl::Status WriteDeclToLayer(ClassTableLayer& layer, ClassDecl const& decl,
                                absl::Nullable<Class const*> prev_decl) {
    return WriteBaseToLayer(layer, decl.base, decl.species, prev_decl);
  }

  SelectorTable const* sel_table_;
  std::vector<ClassDef> defs_;
  std::vector<ClassDecl> decls_;
};
}  // namespace

std::unique_ptr<ClassTableBuilder> ClassTableBuilder::Create(
    SelectorTable const* sel_table) {
  return std::make_unique<ClassTableBuilderImpl>(sel_table);
}

}  // namespace sem