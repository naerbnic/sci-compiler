// define.hpp		sc
//		definitions for define.cpp.

#ifndef DEFINE_HPP
#define DEFINE_HPP

#include <vector>

#include "symbol.hpp"

enum VarType {
  VAR_NONE,
  VAR_GLOBAL,
  VAR_LOCAL,
};

struct Var {
  // Variable definition.
  Var() : type((sym_t)VAR_NONE), value(0) {}

  sym_t type;
  int value;
};

struct VarList {
  // Variable block definition.
  VarList() : fixups(0), type(VAR_NONE), values(0) {}

  void kill();

  int fixups;               // number of fixups in this variable list
  VarType type;             // what type of variables are these
  std::vector<Var> values;  // pointer to block of initial values
};

struct Public {
  // Node for public/export definition lists.

  Public(Symbol* s = 0) : next(0), sym(s), script(0), entry(0) {}

  Public* next;
  Symbol* sym;     // pointer to the relevant symbol
  int script;      // script number
  uint32_t entry;  // index in dispatch table
};

void Define();
void Enum();
void Global();
void Local();
void DoPublic();
Symbol* FindPublic(int);
void Extern();
void InitPublics();
void Definition();

extern VarList globalVars;
extern VarList localVars;
extern int maxVars;

#endif
