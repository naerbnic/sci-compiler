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

//	class.cpp
void DefineClass();
Class* FindClass(int n);
void InstallObjects();
int GetClassNumber(Class*);
Class* NextClass(int n);

extern Class* gClasses[];
extern int gMaxClassNum;

#endif