//	symbol.cpp	sc
// 	symbol class routines for sc

#include "sc.hpp"
#include "sol.hpp"
#include "string.hpp"

// #include "char.hpp"
#include "define.hpp"
#include "input.hpp"
#include "object.hpp"
#include "symbol.hpp"

Symbol::Symbol(const char* name, sym_t type)
    : name_(newStr(name)),
      type(type),
      lineNum(curLine),
      an(0),
      str(0),
      next(0) {}

Symbol::~Symbol() {
  switch (type) {
    case S_DEFINE:
      delete[] str;
      break;

    case S_EXTERN:
      delete ext;
      break;

    case S_OBJ:
      delete obj;
      break;

    default:
      break;
  }

  delete[] name_;
}
