//	token.hpp	sc

#ifndef TOKEN_HPP
#define TOKEN_HPP

#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "scic/symbol.hpp"
#include "scic/symtypes.hpp"

//	token.cpp
void GetToken();
bool NextToken();
bool NewToken();
void UnGetTok();
void GetRest(bool error = false);
bool GetNewLine();

//	toktypes.cpp

// This keeps track of the current token value.
class TokenSlot {
 public:
  using RefVal = std::variant<int, std::string>;

  TokenSlot() : type(S_END), ref_val_(0){};
  TokenSlot(sym_t type, std::string raw_string, int val)
      : type(type), raw_string_(raw_string), ref_val_(val) {}
  TokenSlot(sym_t type, std::string raw_string_, std::string val)
      : type(type), raw_string_(raw_string_), ref_val_(std::move(val)) {}

  TokenSlot(TokenSlot const& tok) = default;
  TokenSlot(TokenSlot&& tok) noexcept = default;
  TokenSlot& operator=(TokenSlot const& tok) = default;
  TokenSlot& operator=(TokenSlot&& tok) noexcept = default;

  sym_t type;

  void SaveSymbol(Symbol const& sym) {
    name_ = sym.name();
    type = sym.type;
    switch (sym.refVal().index()) {
      case 0:
        ref_val_ = sym.val();
        break;

      case 1:
        ref_val_ = std::string(sym.str());
        break;
    }
  }

  std::string_view name() const { return name_; }
  void clearName() { name_.clear(); }

  int val() const {
    auto* ptr = std::get_if<int>(&ref_val_);
    return ptr ? *ptr : 0;
  }
  bool hasVal(int val) const {
    auto* ptr = std::get_if<int>(&ref_val_);
    return ptr && *ptr == val;
  }
  void setVal(int val) { ref_val_ = val; }
  std::string_view str() const {
    auto* ptr = std::get_if<std::string>(&ref_val_);
    return ptr ? ptr->c_str() : "";
  }
  void setStr(std::string_view str) { ref_val_ = std::string(str); }

 private:
  std::string name_;
  std::string raw_string_;
  RefVal ref_val_;
};

const int MaxTokenLen = 2048;

class TokenState {
 public:
  TokenState();

  int& nestedCondCompile() { return nested_cond_compile_; }
  std::string& symStr() { return sym_str_; }
  TokenSlot& tokSym() { return tok_sym_; }

 private:
  int nested_cond_compile_;
  std::string sym_str_;
  TokenSlot tok_sym_;
};

extern TokenState gTokenState;

inline sym_t symType() { return gTokenState.tokSym().type; }
inline void setSymType(sym_t typ) { gTokenState.tokSym().type = typ; }
inline int symVal() { return gTokenState.tokSym().val(); }
inline bool symHasVal(int x) { return gTokenState.tokSym().hasVal(x); }
inline void setSymVal(int x) { gTokenState.tokSym().setVal(x); }

#endif
