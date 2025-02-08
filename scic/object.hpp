// object.hpp
// 	definitions for objects & classes

#ifndef OBJECT_HPP
#define OBJECT_HPP

#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "selector.hpp"

struct ANode;
class Symbol;

struct Class;

struct Object {
  Object();
  Object(Class* theClass);

  void dupSelectors(Class*);
  Selector* findSelectorByNum(int val);
  Selector* findSelector(std::string_view name);
  void freeSelectors();

  auto selectors() const {
    return std::views::transform(selectors_,
                                 [](auto const& p) { return p.get(); });
  }

  Symbol* sym;       // the symbol for this object/class
  int num;           // class number (== OBJECTNUM for objects)
  int super;         // number of this object's super-class
  int script;        // module # in which this object is defined
  int numProps;      // number of properties in object
  ANode* an;         // pointer to object definition
  std::string file;  // filename in which object was defined

 protected:
  std::vector<std::unique_ptr<Selector>> selectors_;  // object's selectors
};

#define OBJECTNUM -1  // Class number for objects.

// Definitions for determining whether a tag refers to a property or method.
#define PROPERTY 0x80
#define PropTag(t) (t & PROPERTY)
#define IsProperty(sn) PropTag((sn)->tag)
#define IsMethod(sn) (!IsProperty(sn))

// Tags for entries in procedure/object dispatch tables.
#define T_PROP (0 | PROPERTY)  // 'v.val' is property value.
#define T_TEXT (1 | PROPERTY)  // 'v.val' is offset in strings
#define T_LOCAL 2              // 'an' is the AsmNode of the method code
#define T_METHOD 3             // this is an inherited (non-local) method
#define T_META (4 | PROPERTY)  // 'v.val' is offset in meta-strings
#define T_PROPDICT \
  (5 | PROPERTY)  // should contain offset of property dictionary
#define T_METHDICT \
  (6 | PROPERTY)  // should contain offset of method
                  //	dictionary

#define CLASSBIT 0x8000

#define SEL_OBJID 0x1000
#define SEL_SIZE 0x1001
#define SEL_PROPDICT 0x1002
#define SEL_METHDICT 0x1003
#define SEL_CLASS_SCRIPT 0x1004
#define SEL_SCRIPT 0x1005
#define SEL_SUPER 0x1006
#define SEL_INFO 0x1007

// Bit to set in the byte indicating number of arguments to a message
// which indicates that the selector should be looked up as a method first.
#define ISMETHOD 0x80

// Special object codes.
#define OBJ_SELF 0xffff    // refers to current object
#define OBJ_SUPER 0xfffe   // refers to object's defining class
#define OBJ_SUPERC 0xfffd  // refers to object's class' super-class

#endif
