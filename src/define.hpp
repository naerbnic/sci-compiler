// define.hpp		sc
//		definitions for define.cpp.

#ifndef DEFINE_HPP
#define DEFINE_HPP

#include <cstdint>
#include <cstddef>
#include <vector>

#include "symtypes.hpp"

class Symbol;

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
  VarList() : type(VAR_NONE), values(0) {}

  void kill();

  VarType type;             // what type of variables are these
  std::vector<Var> values;  // pointer to block of initial values
};

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

extern VarList globalVars;
extern VarList localVars;
extern std::size_t maxVars;

#endif
