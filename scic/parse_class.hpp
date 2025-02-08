#ifndef PARSE_CLASS_HPP
#define PARSE_CLASS_HPP

#include "scic/class.hpp"
#include "scic/parse_context.hpp"

void DefineClass();
Class* FindClass(int n);
void InstallObjects(ParseContext* parseContext);
int GetClassNumber(Class*);
Class* NextClass(int n);

#endif