//	object.cpp
// 	handle class code/instances.

#include "scic/legacy/object.hpp"

#include <memory>
#include <string_view>
#include <utility>

#include "scic/legacy/class.hpp"
#include "scic/legacy/compile.hpp"
#include "scic/legacy/selector.hpp"
#include "scic/legacy/symbol.hpp"
#include "scic/legacy/symtbl.hpp"

Object::Object() : sym(0), num(0), super(0), numProps(0) {}

Object::Object(Class* theSuper) : sym(0), num(0), super(0), numProps(0) {
  super = theSuper->num;
  dupSelectors(theSuper);
}

void Object::dupSelectors(Class* super) {
  // duplicate super's selectors

  for (auto* sn : super->selectors()) {
    auto tn = std::make_unique<Selector>();
    *tn = *sn;
    if (tn->tag == T_LOCAL) {
      tn->tag = T_METHOD;  // No longer a local method.
    }
    selectors_.push_back(std::move(tn));
  }

  numProps = super->numProps;
}

Selector* Object::findSelectorByNum(int val) {
  // Return a pointer to the selector node which corresponds to the
  //	symbol 'sym'.

  for (auto* sn : selectors()) {
    if (val == sn->sym->val()) return sn;
  }

  return nullptr;
}

Selector* Object::findSelector(std::string_view name) {
  // Return a pointer to the selector node which has the name 'name'.

  Symbol* sym = gSyms.lookup(name);
  return sym ? findSelectorByNum(sym->val()) : 0;
}

void Object::freeSelectors() {
  // free the object's selectors

  selectors_.clear();
}

///////////////////////////////////////////////////////////////////////////
