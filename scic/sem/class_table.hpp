#ifndef SEM_CLASS_TABLE_HPP
#define SEM_CLASS_TABLE_HPP

#include <MacTypes.h>

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/selector_table.hpp"
#include "util/strings/ref_str.hpp"

namespace sem {

class Class {
 public:
  class Property {
   public:
    virtual ~Property() = default;

    virtual NameToken const& token_name() const = 0;
    virtual util::RefStr const& name() const = 0;
    virtual SelectorTable::Entry const* selector() const = 0;
    virtual codegen::LiteralValue value() const = 0;
  };

  class Method {
   public:
    virtual ~Method() = default;

    virtual NameToken const& token_name() const = 0;
    virtual util::RefStr const& name() const = 0;
    virtual SelectorTable::Entry const* selector() const = 0;
  };

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

  // The properties for this class.
  virtual std::vector<Property const*> const& properties() const = 0;

  // The methods for this class.
  virtual std::vector<Method const*> const& methods() const = 0;
};

class ClassTable {
 public:
  virtual ~ClassTable() = default;

  virtual absl::Nullable<Class const*> LookupBySpecies(
      ClassSpecies species) const = 0;
  virtual absl::Nullable<Class const*> LookupByName(
      std::string_view name) const = 0;

  // Same as above functions, but only performs lookup in the context of the
  // original script declarations.
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

  virtual absl::Status AddClassDecl(NameToken name, ScriptNum script_num,
                                    ClassSpecies species,
                                    std::optional<ClassSpecies> super_species,
                                    std::vector<Property> properties,
                                    std::vector<NameToken> methods) = 0;

  virtual absl::Status AddClassDef(NameToken name, ScriptNum script_num,
                                   std::optional<NameToken> super_name,
                                   std::vector<Property> properties,
                                   std::vector<NameToken> methods) = 0;

  virtual absl::StatusOr<std::unique_ptr<ClassTable>> Build() = 0;
};

}  // namespace sem
#endif