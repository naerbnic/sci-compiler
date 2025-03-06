#ifndef SEM_CLASS_TABLE_HPP
#define SEM_CLASS_TABLE_HPP

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "absl/base/nullability.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/obj_members.hpp"
#include "scic/sem/property_list.hpp"
#include "scic/sem/selector_table.hpp"
#include "scic/status/status.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/sequence.hpp"

namespace sem {

class Class {
 public:
  virtual ~Class() = default;

  // Returns the name of this class, along with the source location it was
  // defined at.
  virtual NameToken const& token_name() const = 0;

  // Returns just the name of this class.
  virtual util::RefStr const& name() const = 0;

  // The script number of the script that declared this class.
  virtual ScriptNum script_num() const = 0;

  // The species of this class.
  virtual ClassSpecies species() const = 0;

  // Returns the class that this class inherits from, or null if it has no
  // superclass.
  virtual absl::Nullable<Class const*> super() const = 0;

  // Returns the class that was originally declared (with dependencies
  // resolved based on declarations alone), or nullptr if this class was not
  // previously declared.
  virtual absl::Nullable<Class const*> prev_decl() const = 0;

  // Returns the PtrRef to the class in its module.
  //
  // This pointer is generated in the module the class was defined in, if
  // it was defined in a module. This _must not_ be used in other
  // codegen instances.
  //
  // Returns nullptr if this class was not defined in a module.
  virtual absl::Nullable<codegen::PtrRef*> class_ref() const = 0;

  virtual std::size_t prop_size() const = 0;

  virtual PropertyList const& prop_list() const = 0;

  // The methods for this class.
  virtual util::Seq<Method const&> methods() const = 0;

  virtual Method const* LookupMethByName(std::string_view name) const = 0;
};

class ClassTable {
 public:
  virtual ~ClassTable() = default;

  virtual util::Seq<Class const&> classes() const = 0;
  virtual absl::Nullable<Class const*> LookupBySpecies(
      ClassSpecies species) const = 0;
  virtual absl::Nullable<Class const*> LookupByName(
      std::string_view name) const = 0;

  // Same as above functions, but only performs lookup in the context of the
  // original script declarations.
  virtual util::Seq<Class const&> decl_classes() const = 0;
  virtual absl::Nullable<Class const*> LookupDeclBySpecies(
      ClassSpecies species) const = 0;
  virtual absl::Nullable<Class const*> LookupDeclByName(
      std::string_view name) const = 0;
};

class ClassTableBuilder {
 public:
  struct Property {
    NameToken name;
    codegen::LiteralValue value;
  };

  static std::unique_ptr<ClassTableBuilder> Create(
      SelectorTable const* sel_table);

  virtual ~ClassTableBuilder() = default;

  virtual status::Status AddClassDecl(NameToken name, ScriptNum script_num,
                                      ClassSpecies species,
                                      std::optional<ClassSpecies> super_species,
                                      std::vector<Property> properties,
                                      std::vector<NameToken> methods) = 0;

  virtual status::Status AddClassDef(NameToken name, ScriptNum script_num,
                                     std::optional<NameToken> super_name,
                                     std::vector<Property> properties,
                                     std::vector<NameToken> methods,
                                     codegen::PtrRef class_ref) = 0;

  virtual status::StatusOr<std::unique_ptr<ClassTable>> Build() = 0;
};

}  // namespace sem
#endif