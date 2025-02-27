#ifndef SEM_OBJ_MEMBERS_HPP
#define SEM_OBJ_MEMBERS_HPP

#include "scic/codegen/code_generator.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/selector_table.hpp"
#include "util/strings/ref_str.hpp"

namespace sem {

class Property {
 public:
  virtual ~Property() = default;

  virtual NameToken const& token_name() const = 0;
  virtual PropIndex index() const = 0;
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

}  // namespace sem
#endif