#ifndef SEM_OBJECT_TABLE
#define SEM_OBJECT_TABLE

#include <memory>
#include <string_view>
#include <vector>

#include "scic/codegen/code_generator.hpp"
#include "scic/sem/class_table.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/obj_members.hpp"
#include "scic/sem/property_list.hpp"
#include "scic/sem/selector_table.hpp"
#include "scic/status/status.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/sequence.hpp"

namespace sem {

class Object {
 public:
  virtual ~Object() = default;

  virtual ScriptNum script_num() const = 0;
  virtual NameToken const& token_name() const = 0;
  virtual util::RefStr const& name() const = 0;
  virtual Class const* parent() const = 0;
  // Gets the codegen::PtrRef for this object.
  virtual codegen::PtrRef* ptr_ref() const = 0;
  virtual PropertyList const& prop_list() const = 0;
  virtual util::Seq<Method const&> methods() const = 0;
};

class ObjectTable {
 public:
  virtual ~ObjectTable() = default;
  virtual util::Seq<Object const&> objects(ScriptNum script) const = 0;
  virtual Object const* LookupByName(std::string_view objName) const = 0;
};

class ObjectTableBuilder {
 public:
  struct Property {
    NameToken name;
    codegen::LiteralValue value;
  };

  static std::unique_ptr<ObjectTableBuilder> Create(
      codegen::CodeGenerator* codegen, SelectorTable const* selector,
      ClassTable const* class_table);

  virtual ~ObjectTableBuilder() = default;

  virtual status::Status AddObject(NameToken name, ScriptNum script_num,
                                   NameToken class_name,
                                   std::vector<Property> properties,
                                   std::vector<NameToken> methods) = 0;

  virtual status::StatusOr<std::unique_ptr<ObjectTable>> Build() = 0;
};

}  // namespace sem

#endif