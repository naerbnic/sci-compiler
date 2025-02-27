#include "scic/sem/object_table.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/sem/class_table.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/obj_members.hpp"
#include "scic/sem/selector_table.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/sequence.hpp"

namespace sem {
namespace {
class PropertyImpl : public Property {
 public:
  PropertyImpl(NameToken name, PropIndex prop_index,
               SelectorTable::Entry const* selector,
               codegen::LiteralValue value)
      : name_(std::move(name)),
        prop_index_(prop_index),
        selector_(selector),
        value_(value) {}

  NameToken const& token_name() const override { return name_; }
  util::RefStr const& name() const override { return name_.value(); }
  PropIndex index() const override { return prop_index_; }
  SelectorTable::Entry const* selector() const override { return selector_; }
  codegen::LiteralValue value() const override { return value_; }

 private:
  NameToken name_;
  PropIndex prop_index_;
  SelectorTable::Entry const* selector_;
  codegen::LiteralValue value_;
};

class MethodImpl : public Method {
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

class ObjectImpl : public Object {
 public:
  ObjectImpl(NameToken name, ScriptNum script_num, Class const* parent,
             codegen::PtrRef ptr_ref, std::vector<PropertyImpl> properties,
             std::vector<MethodImpl> methods)
      : name_(std::move(name)),
        script_num_(script_num),
        parent_(parent),
        ptr_ref_(std::move(ptr_ref)) {}

  ScriptNum script_num() const override { return script_num_; }
  NameToken const& token_name() const override { return name_; }
  util::RefStr const& name() const override { return name_.value(); }
  Class const* parent() const override { return parent_; }
  codegen::PtrRef* ptr_ref() const override { return &ptr_ref_; }
  util::Seq<Property const&> properties() const override { return properties_; }
  util::Seq<Method const&> methods() const override { return methods_; }

  Property const* LookupPropByName(std::string_view name) const override {
    for (auto const& prop : properties_) {
      if (prop.name() == name) {
        return &prop;
      }
    }
    return nullptr;
  }

 private:
  NameToken name_;
  ScriptNum script_num_;
  Class const* parent_;
  mutable codegen::PtrRef ptr_ref_;
  std::vector<PropertyImpl> properties_;
  std::vector<MethodImpl> methods_;
};

class ObjectTableImpl : public ObjectTable {
 public:
  ObjectTableImpl(
      std::vector<std::unique_ptr<ObjectImpl>> objects,
      std::map<std::string_view, ObjectImpl*, std::less<>> name_table)
      : objects_(std::move(objects)), name_table_(std::move(name_table)) {}

  Object const* LookupByName(std::string_view objName) const override {
    auto it = name_table_.find(objName);
    if (it == name_table_.end()) {
      return nullptr;
    }
    return it->second;
  }

  util::Seq<Object const&> objects(ScriptNum script) const override {
    return util::Seq<Object const&>::Deref(objects_);
  }

 private:
  std::vector<std::unique_ptr<ObjectImpl>> objects_;
  std::map<std::string_view, ObjectImpl*, std::less<>> name_table_;
};

class ObjectTableBuilderImpl : public ObjectTableBuilder {
 public:
  ObjectTableBuilderImpl(codegen::CodeGenerator* codegen,
                         SelectorTable const* selector,
                         ClassTable const* class_table)
      : codegen_(codegen), selector_(selector), class_table_(class_table) {}

  absl::Status AddObject(NameToken name, ScriptNum script_num,
                         NameToken class_name, std::vector<Property> properties,
                         std::vector<NameToken> methods) override {
    auto const* class_ = class_table_->LookupByName(class_name.value());
    if (!class_) {
      return absl::InvalidArgumentError("Class not found");
    }

    auto ptr_ref = codegen_->CreatePtrRef();

    std::vector<PropertyImpl> prop_impls;
    for (auto const& prop : properties) {
      auto const* selector = selector_->LookupByName(prop.name.value());
      if (!selector) {
        return absl::InvalidArgumentError("Selector not found");
      }
      auto const* class_prop = class_->LookupPropByName(prop.name.value());
      if (!class_prop) {
        return absl::InvalidArgumentError("Property not found in superclass");
      }
      prop_impls.emplace_back(prop.name, class_prop->index(), selector,
                              prop.value);
    }

    std::vector<MethodImpl> method_impls;
    for (auto const& method : methods) {
      auto const* selector = selector_->LookupByName(method.value());
      if (!selector) {
        return absl::InvalidArgumentError("Selector not found");
      }
      method_impls.emplace_back(method, selector);
    }
    auto new_object = std::make_unique<ObjectImpl>(
        name, script_num, class_, std::move(ptr_ref), std::move(prop_impls),
        std::move(method_impls));

    name_table_.emplace(new_object->name(), new_object.get());
    objects_.push_back(std::move(new_object));
    return absl::OkStatus();
  }

  absl::StatusOr<std::unique_ptr<ObjectTable>> Build() override {
    return std::make_unique<ObjectTableImpl>(std::move(objects_),
                                             std::move(name_table_));
  }

 private:
  absl::Nonnull<codegen::CodeGenerator*> codegen_;
  absl::Nonnull<SelectorTable const*> selector_;
  absl::Nonnull<ClassTable const*> class_table_;
  std::vector<std::unique_ptr<ObjectImpl>> objects_;
  std::map<std::string_view, ObjectImpl*, std::less<>> name_table_;
};
}  // namespace

std::unique_ptr<ObjectTableBuilder> ObjectTableBuilder::Create(
    codegen::CodeGenerator* codegen, SelectorTable const* selector,
    ClassTable const* class_table) {
  return std::make_unique<ObjectTableBuilderImpl>(codegen, selector,
                                                  class_table);
}

}  // namespace sem