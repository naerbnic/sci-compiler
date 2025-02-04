//	token.hpp	sc

#ifndef TOKEN_HPP
#define TOKEN_HPP

#include <string_view>

#include "symbol.hpp"

//	token.cpp
void GetToken();
bool NextToken();
bool NewToken();
void UnGetTok();
void GetRest(bool error = false);
bool GetNewLine();

//	toktypes.cpp
Symbol* LookupTok();
Symbol* GetSymbol();
bool GetNumber(std::string_view);
bool GetNumberOrString(std::string_view);
bool GetString(std::string_view);
bool GetIdent();
bool GetDefineSymbol();
bool IsIdent();
bool IsUndefinedIdent();
keyword_t Keyword();
void GetKeyword(keyword_t);
bool IsVar();
bool IsProc();
bool IsObj();
bool IsNumber();

// This keeps track of the current token value.
class TokenSlot {
 public:
  using RefVal = std::variant<int, std::string>;

  TokenSlot() : type(S_END), ref_val_(0) {};
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

#define symObj gTokSym.obj()
#define symType gTokSym.type
#define symVal gTokSym.val()
#define symHasVal(x) gTokSym.hasVal(x)
#define setSymVal(x) gTokSym.setVal(x)

const int MaxTokenLen = 2048;

extern int gNestedCondCompile;
extern int gSelectorIsVar;
extern std::string gSymStr;
extern TokenSlot gTokSym;

#endif
