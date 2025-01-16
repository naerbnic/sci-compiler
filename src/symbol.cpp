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
    : name_(name ? std::optional(std::string(name)) : std::nullopt),
      type(type),
      lineNum(curLine),
      an_(0),
      str_(0),
      next(0) {}

Symbol::~Symbol() {
  switch (type) {
    case S_DEFINE:
      delete[] str_;
      break;

    case S_EXTERN:
      delete ext_;
      break;

    case S_OBJ:
      delete obj_;
      break;

    default:
      break;
  }
}
