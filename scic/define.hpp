// define.hpp		sc
//		definitions for define.cpp.

#ifndef DEFINE_HPP
#define DEFINE_HPP

#include "scic/public.hpp"
#include "scic/selector.hpp"

void Define();
void Enum();
void GlobalDecl();
void Global();
void Local();
void DoPublic();
Symbol* FindPublic(PublicList const& publicList, int);
void Extern();
void InitPublics();
void Definition();

#endif
