#ifndef PARSE_OBJECT_HPP
#define PARSE_OBJECT_HPP

#include "scic/object.hpp"

void DoClass();
void Instance();

extern Object* gCurObj;
extern Symbol* gNameSymbol;
extern Object* gReceiver;

#endif