// symbol.hpp
//		definitions for symbols

#ifndef SYMBOL_HPP
#define SYMBOL_HPP

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <variant>

#include "define.hpp"
#include "object.hpp"
#include "sc.hpp"
#include "symtypes.hpp"

struct ANode;
struct ANReference;
struct Public;

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

 private:
  std::variant<ANode*, ANReference*>
      sym_value_;  // pointer to symbol definition in the AList

  RefVal ref_val_;

 public:
  strptr name() const { return name_ ? name_->c_str() : nullptr; }
  void clearName() { name_ = std::nullopt; }

  ANode* an() {
    auto* ptr = std::get_if<0>(&sym_value_);
    return ptr ? *ptr : nullptr;
  }
  void clearAn() { sym_value_ = static_cast<ANode*>(nullptr); }
  ANode* loc() {
    auto* ptr = std::get_if<0>(&sym_value_);
    return ptr ? *ptr : nullptr;
  }
  void setLoc(ANode* loc) { sym_value_ = loc; }
  ANReference* ref() {
    auto* ptr = std::get_if<1>(&sym_value_);
    return ptr ? *ptr : nullptr;
  }
  void setRef(ANReference* ref) { sym_value_ = ref; }

  RefVal& refVal() { return ref_val_; }
  RefVal const& refVal() const { return ref_val_; }
  void setRefVal(RefVal ref_val) { ref_val_ = std::move(ref_val); }

  int val() const;
  bool hasVal(int val) const;
  void setVal(int val);
  strptr str() const;
  void setStr(strptr str);
  Object* obj() const;
  void setObj(std::unique_ptr<Object> obj);
  Public* ext() const;
  void setExt(std::unique_ptr<Public> ext);

 private:
  friend std::ostream& operator<<(std::ostream& os, const Symbol& sym);
};

inline constexpr auto OPEN_P = S_OPEN_P;
inline constexpr auto OPEN_B = ((sym_t)'{');
inline constexpr auto CLOSE_P = ((sym_t)')');
inline constexpr auto CLOSE_B = ((sym_t)'}');
inline bool OpenP(sym_t c) { return c == OPEN_P; }
inline bool CloseP(sym_t c) { return c == CLOSE_P; }

#define KERNEL -1  // Module # of kernel

#endif
