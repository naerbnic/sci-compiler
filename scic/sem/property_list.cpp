#include "scic/sem/property_list.hpp"

#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "scic/codegen/code_generator.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/obj_members.hpp"
#include "scic/sem/selector_table.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/sequence.hpp"

namespace sem {

class PropertyList::PropertyImpl : public Property {
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

// ---------------------------------------------------------------

// Define the destructor here to ensure PropertyImpl is defined at this point.
PropertyList::PropertyList() = default;
PropertyList::~PropertyList() = default;

PropertyList::PropertyList(PropertyList&&) = default;
PropertyList& PropertyList::operator=(PropertyList&&) = default;

// Adds a property definition to the list. If a property with the same name
// already exists, it is updated with the new value.
//
// Returns the index of the property in the list.
//
// The order of evaluation of this method is significant. For each new
// property, it will be appended to the current list of properties.
PropIndex PropertyList::UpdatePropertyDef(NameToken name,
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

PropIndex PropertyList::UpdatePropertyDef(SelectorTable::Entry const* selector,
                                          codegen::LiteralValue value) {
  return UpdatePropertyDef(selector->name_token(), selector, std::move(value));
}

PropertyList PropertyList::Clone() const {
  std::vector<std::unique_ptr<PropertyImpl>> new_properties;
  for (auto& prop : properties_) {
    new_properties.push_back(std::make_unique<PropertyImpl>(*prop));
  }
  return PropertyList(std::move(new_properties));
}

util::Seq<Property const&> PropertyList::properties() const {
  return util::Seq<Property const&>::Deref(properties_);
}
std::size_t PropertyList::size() const { return properties_.size(); }
Property const* PropertyList::LookupByName(std::string_view name) const {
  auto it = name_table_.find(name);
  if (it == name_table_.end()) {
    return nullptr;
  }
  return it->second;
}

PropertyList::PropertyList(
    std::vector<std::unique_ptr<PropertyImpl>> properties)
    : properties_(std::move(properties)) {
  for (auto& prop : properties_) {
    AddToIndex(prop.get());
  }
}

void PropertyList::AddToIndex(PropertyImpl* prop) {
  name_table_.emplace(prop->name(), prop);
  selector_table_.emplace(prop->selector(), prop);
  index_table_.emplace(prop->index(), prop);
}

}  // namespace sem