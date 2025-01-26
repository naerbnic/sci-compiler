//	symbol.cpp	sc
// 	symbol class routines for sc

#include "symbol.hpp"

#include "define.hpp"
#include "input.hpp"
#include "object.hpp"

Symbol::Symbol(std::string_view name, sym_t type)
    : name_(!name.empty() ? std::optional(std::string(name)) : std::nullopt),
      type(type),
      lineNum(curLine),
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

std::ostream& operator<<(std::ostream& os, const Symbol& sym) {
  return os << "Symbol(" << sym.name() << ")";
}
