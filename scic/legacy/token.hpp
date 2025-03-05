//	token.hpp	sc

#ifndef TOKEN_HPP
#define TOKEN_HPP

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"
#include "scic/legacy/symbol.hpp"
#include "scic/legacy/symtypes.hpp"

// This keeps track of the current token value.
class TokenSlot {
 public:
  TokenSlot() : type_(S_END), ref_val_(0){};
  TokenSlot(sym_t type, std::string name)
      : type_(type), name_(std::move(name)) {}
  TokenSlot(sym_t type, std::string name, int val)
      : type_(type), name_(std::move(name)), ref_val_(val) {}

  TokenSlot(TokenSlot const& tok) = default;
  TokenSlot(TokenSlot&& tok) noexcept = default;
  TokenSlot& operator=(TokenSlot const& tok) = default;
  TokenSlot& operator=(TokenSlot&& tok) noexcept = default;

  void SaveSymbol(Symbol const& sym) {
    name_ = sym.name();
    type_ = sym.type;
    switch (sym.refVal().index()) {
      case 0:
        ref_val_ = sym.val();
        break;
    }
  }

  sym_t type() const { return type_; }
  void setType(sym_t type) { type_ = type; }
  std::string_view name() const { return name_; }
  void clearName() { name_.clear(); }

  int val() const { return ref_val_.value_or(0); }
  bool hasVal(int val) const { return ref_val_.has_value(); }
  void setVal(int val) { ref_val_ = val; }

 private:
  sym_t type_;
  std::string name_;
  std::optional<int> ref_val_;

  template <class Sink>
  friend void AbslStringify(Sink& sink, TokenSlot const& token) {
    if (token.ref_val_.has_value()) {
      absl::Format(&sink, "TokenSlot(type: %d, name: \"%s\", val: %d)",
                   token.type_, absl::CEscape(token.name_),
                   token.ref_val_.value());
    } else {
      absl::Format(&sink, "TokenSlot(type: %d, name: \"%s\")", token.type_,
                   absl::CEscape(token.name_));
    }
  }
};

const int MaxTokenLen = 2048;

class TokenState {
 public:
  TokenState();

  int& nestedCondCompile() { return nested_cond_compile_; }

 private:
  int nested_cond_compile_;
};

extern TokenState gTokenState;

//	token.cpp
[[nodiscard]] TokenSlot GetToken();
[[nodiscard]] std::optional<TokenSlot> NextToken();
[[nodiscard]] std::optional<TokenSlot> NewToken();
void UnGetTok();
[[nodiscard]] std::optional<TokenSlot> GetRest(bool error = false);
bool GetNewLine();

#endif
