// define.hpp		sc
//		definitions for define.cpp.

#ifndef DEFINE_HPP
#define DEFINE_HPP

#include <cstdint>

class Symbol;

struct Public {
  // Node for public/export definition lists.

  Public(Symbol* s = 0) : sym(s), script(0), entry(0) {}

  Symbol* sym;     // pointer to the relevant symbol
  int script;      // script number
  uint32_t entry;  // index in dispatch table
};

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
