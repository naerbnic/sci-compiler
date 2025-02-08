//	token.hpp	sc

#ifndef TOKEN_HPP
#define TOKEN_HPP

#include <string>
#include <string_view>
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
  TokenSlot(TokenSlot const& tok) = default;
  TokenSlot& operator=(TokenSlot const& tok) = default;

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
  RefVal ref_val_;
};

const int MaxTokenLen = 2048;

extern int gNestedCondCompile;
extern std::string gSymStr;
extern TokenSlot gTokSym;

inline sym_t symType() { return gTokSym.type; }
inline void setSymType(sym_t typ) { gTokSym.type = typ; }
inline int symVal() { return gTokSym.val(); }
inline bool symHasVal(int x) { return gTokSym.hasVal(x); }
inline void setSymVal(int x) { gTokSym.setVal(x); }

#endif
