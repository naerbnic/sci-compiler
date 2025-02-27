#ifndef SEM_PROPERTY_LIST_HPP
#define SEM_PROPERTY_LIST_HPP

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string_view>
#include <vector>

#include "scic/codegen/code_generator.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/obj_members.hpp"
#include "scic/sem/selector_table.hpp"
#include "util/types/sequence.hpp"
namespace sem {

// A full property list for a class.
//
// This includes all properties, including those inherited from super classes.
//
// When an object or class is created, all of its properties are laid out in
// memory, and are used both for initialization and for memory storage, so all
// properties must be known at that time.
class PropertyList {
 public:
  // Implemented as default, but defined inside cpp file.
  PropertyList();
  ~PropertyList();
  PropertyList(PropertyList&&);
  PropertyList& operator=(PropertyList&&);

  PropertyList(PropertyList const&) = delete;
  PropertyList& operator=(PropertyList const&) = delete;

  // Adds a property definition to the list. If a property with the same name
  // already exists, it is updated with the new value.
  //
  // Returns the index of the property in the list.
  //
  // The order of evaluation of this method is significant. For each new
  // property, it will be appended to the current list of properties.
  PropIndex UpdatePropertyDef(NameToken name,
                              SelectorTable::Entry const* selector,
                              codegen::LiteralValue value);

  PropIndex UpdatePropertyDef(SelectorTable::Entry const* selector,
                              codegen::LiteralValue value);

  PropertyList Clone() const;

  util::Seq<Property const&> properties() const;
  std::size_t size() const;
  Property const* LookupByName(std::string_view name) const;

 private:
  class PropertyImpl;

  PropertyList(std::vector<std::unique_ptr<PropertyImpl>> properties);

  void AddToIndex(PropertyImpl* prop);

  std::vector<std::unique_ptr<PropertyImpl>> properties_;
  std::map<std::string_view, PropertyImpl*, std::less<>> name_table_;
  std::map<SelectorTable::Entry const*, PropertyImpl*, std::less<>>
      selector_table_;
  std::map<PropIndex, PropertyImpl*, std::less<>> index_table_;
};
}  // namespace sem

#endif