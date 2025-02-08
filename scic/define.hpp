// define.hpp		sc
//		definitions for define.cpp.

#ifndef DEFINE_HPP
#define DEFINE_HPP

#include "scic/selector.hpp"

void Define();
void Enum();
void GlobalDecl();
void Global();
void Local();
void DoPublic();
Symbol* FindPublic(int);
void Extern();
void InitPublics();
void Definition();

#endif
