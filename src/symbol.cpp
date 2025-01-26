//	symbol.cpp	sc
// 	symbol class routines for sc

#include "symbol.hpp"

#include "define.hpp"
#include "input.hpp"
#include "object.hpp"

Symbol::Symbol(std::string_view name, sym_t type)
    : name_(!name.empty() ? std::optional(std::string(name)) : std::nullopt),
      type(type),
      lineNum(curLine) {}

Symbol::~Symbol() {
  switch (ref_val_.index()) {
    case 1:
      if (type == S_DEFINE) {
        delete[] std::get<1>(ref_val_);
      }
      break;

    case 2:
      delete std::get<2>(ref_val_);
      break;

    case 3:
      delete std::get<3>(ref_val_);
      break;

    default:
      break;
  }
}

int Symbol::val() const { return std::get<0>(ref_val_); }
bool Symbol::hasVal(int val) const {
  return std::holds_alternative<int>(ref_val_) && std::get<0>(ref_val_) == val;
}
void Symbol::setVal(int val) { ref_val_ = val; }
strptr Symbol::str() const { return std::get<1>(ref_val_); }
void Symbol::setStr(strptr str) { ref_val_ = str; }
Object* Symbol::obj() const {
  auto ptr = std::get_if<2>(&ref_val_);
  return ptr ? *ptr : nullptr;
}
void Symbol::setObj(std::unique_ptr<Object> obj) { ref_val_ = obj.release(); }
Public* Symbol::ext() const { return std::get<3>(ref_val_); }
void Symbol::setExt(std::unique_ptr<Public> ext) { ref_val_ = ext.release(); }

std::ostream& operator<<(std::ostream& os, const Symbol& sym) {
  return os << "Symbol(" << sym.name() << ")";
}
