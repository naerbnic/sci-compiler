#ifndef VARLIST_HPP
#define VARLIST_HPP

#include <vector>

#include "scic/symtypes.hpp"

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
  VarList() : type(VAR_NONE) {}

  void kill();

  VarType type;             // what type of variables are these
  std::vector<Var> values;  // pointer to block of initial values
};

#endif