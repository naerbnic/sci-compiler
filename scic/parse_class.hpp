#ifndef PARSE_CLASS_HPP
#define PARSE_CLASS_HPP

#include "scic/class.hpp"

extern Class* gClasses[];
extern int gMaxClassNum;

void DefineClass();
Class* FindClass(int n);
void InstallObjects();
int GetClassNumber(Class*);
Class* NextClass(int n);

#endif