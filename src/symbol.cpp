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
      str_(0) {}

Symbol::~Symbol() {
  switch (type) {
    case S_DEFINE:
      delete[] str_;
      break;

    case S_EXTERN:
      delete ext_;
      break;

    case S_OBJ: {
      // OBJ_SELF and OBJ_SUPER are special values that should not be deleted.
      if (val_ == OBJ_SELF || val_ == OBJ_SUPER) break;
      delete obj_;
      break;
    }

    default:
      break;
  }
}
