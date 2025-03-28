// symbol.hpp
//		definitions for symbols

#ifndef SYMBOL_HPP
#define SYMBOL_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/legacy/global_compiler.hpp"
#include "scic/legacy/object.hpp"
#include "scic/legacy/public.hpp"
#include "scic/legacy/symtypes.hpp"

class Symbol {
  // The Symbol class is where information about identifiers resides.  Symbols
  // are collected in SymTbls for fast lookup of an identifier.
 public:
  using RefVal = std::variant<int, std::string, std::unique_ptr<Object>,
                              std::unique_ptr<Public>>;

  Symbol(std::string_view str = "", sym_t = (sym_t)0);

 private:
  std::optional<std::string> name_;  // pointer to the symbol name

 public:
  sym_t type;        // symbol type
  uint32_t lineNum;  //	where symbol was first defined

  codegen::PtrRef forwardRef;

 private:
  RefVal ref_val_;

 public:
  std::string_view name() const {
    return name_ ? std::string_view(*name_) : "";
  }
  void clearName() { name_ = std::nullopt; }

  void clearAn() { forwardRef = gSc->CreatePtrRef(); }

  RefVal& refVal() { return ref_val_; }
  RefVal const& refVal() const { return ref_val_; }
  void setRefVal(RefVal ref_val) { ref_val_ = std::move(ref_val); }

  int val() const;
  bool hasVal(int val) const;
  void setVal(int val);
  std::string_view str() const;
  void setStr(std::string str);
  Object* obj() const;
  void setObj(std::unique_ptr<Object> obj);
  Public* ext() const;
  void setExt(std::unique_ptr<Public> ext);

 private:
  template <class Sink>
  friend void AbslStringify(Sink& sink, Symbol const& sym) {
    if (sym.ref_val_.index() == 0) {
      absl::Format(&sink, "Symbol(type: %d, name: \"%s\", val: %d)", sym.type,
                   absl::CEscape(sym.name()), sym.val());
    } else {
      absl::Format(&sink, "Symbol(type: %d, name: \"%s\")", sym.type,
                   absl::CEscape(sym.name()));
    }
  }
  friend std::ostream& operator<<(std::ostream& os, const Symbol& sym);
};

inline constexpr auto OPEN_P = S_OPEN_P;
inline constexpr auto OPEN_B = ((sym_t)'{');
inline constexpr auto CLOSE_P = ((sym_t)')');
inline constexpr auto CLOSE_B = ((sym_t)'}');
inline bool OpenP(sym_t c) { return c == OPEN_P; }
inline bool CloseP(sym_t c) { return c == CLOSE_P; }

#endif
