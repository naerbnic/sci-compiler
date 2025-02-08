//	class.cpp
// 	code to deal with classes.

#include "scic/class.hpp"

#include <fcntl.h>
#include <sys/stat.h>

#include <cstdint>
#include <memory>
#include <utility>

#include "scic/object.hpp"
#include "scic/selector.hpp"
#include "scic/symbol.hpp"

Class::Class() : subClasses(0), nextSibling(0) {}

Class::Class(Class* theSuper)
    : Object(theSuper), subClasses(0), nextSibling(0) {
  //	add this class to the end of the super's children

  Class** p;
  for (p = &theSuper->subClasses; *p; p = &(*p)->nextSibling)
    ;
  *p = this;
}

bool Class::selectorDiffers(Selector* tp) {
  // Return true if either the selector referred to by 'tp' is not in
  // this class or if its value differs.

  Selector* stp;

  if (num == -1) return true;

  stp = findSelectorByNum(tp->sym->val());
  return !stp || (IsMethod(tp) && tp->tag == T_LOCAL) ||
         (tp->tag == T_PROP && tp->val != stp->val);
}

Selector* Class::addSelector(Symbol* sym, int what) {
  // Add a selector ('sym') to the selectors for this class.
  // Allocate a selector node, and link it into the selector list for
  // the class.  Finally, return a pointer to the selector node.

  if (!sym) return 0;

  auto sn_owned = std::make_unique<Selector>(sym);
  auto* sn = sn_owned.get();
  selectors_.push_back(std::move(sn_owned));

  switch (sym->val()) {
    case SEL_METHDICT:
      sn->tag = T_METHDICT;
      break;
    case SEL_PROPDICT:
      sn->tag = T_PROPDICT;
      break;
    default:
      sn->tag = (uint8_t)what;
      break;
  }

  if (PropTag(what)) {
    sn->ofs = 2 * numProps;
    ++numProps;
  }

  return sn;
}
