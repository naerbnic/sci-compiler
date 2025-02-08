#ifndef CLASS_HPP
#define CLASS_HPP

#include "scic/object.hpp"
#include "scic/selector.hpp"

struct Class : Object {
  Class();
  Class(Class* theClass);

  Selector* addSelector(Symbol* sym, int what);
  bool selectorDiffers(Selector* tp);

  Class* subClasses;
  Class* nextSibling;
};

#endif