//	symbol.cpp	sc
// 	symbol class routines for sc

#include "scic/symbol.hpp"

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "scic/input.hpp"
#include "scic/object.hpp"
#include "scic/symtypes.hpp"

Symbol::Symbol(std::string_view name, sym_t type)
    : name_(!name.empty() ? std::optional(std::string(name)) : std::nullopt),
      type(type),
      lineNum(gInputState.GetCurrLineNum()) {}

int Symbol::val() const { return std::get<0>(ref_val_); }
bool Symbol::hasVal(int val) const {
  return std::holds_alternative<int>(ref_val_) && std::get<0>(ref_val_) == val;
}
void Symbol::setVal(int val) { ref_val_ = val; }
std::string_view Symbol::str() const { return std::get<1>(ref_val_); }
void Symbol::setStr(std::string str) { ref_val_ = std::move(str); }
Object* Symbol::obj() const {
  auto* ptr = std::get_if<2>(&ref_val_);
  return ptr ? ptr->get() : nullptr;
}
void Symbol::setObj(std::unique_ptr<Object> obj) { ref_val_ = std::move(obj); }
Public* Symbol::ext() const { return std::get<3>(ref_val_).get(); }
void Symbol::setExt(std::unique_ptr<Public> ext) { ref_val_ = std::move(ext); }

std::ostream& operator<<(std::ostream& os, const Symbol& sym) {
  return os << "Symbol(" << sym.name() << ")";
}
