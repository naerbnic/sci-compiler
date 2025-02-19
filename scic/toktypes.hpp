#ifndef TOKTYPES_HPP
#define TOKTYPES_HPP

#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <variant>

#include "scic/codegen/code_generator.hpp"
#include "scic/symbol.hpp"
#include "scic/symtypes.hpp"
#include "scic/token.hpp"

class ResolvedTokenSlot {
 public:
  static ResolvedTokenSlot OfToken(TokenSlot slot) {
    return ResolvedTokenSlot(std::move(slot));
  }
  static ResolvedTokenSlot OfSymbol(Symbol* sym) {
    return ResolvedTokenSlot(sym);
  }

  ResolvedTokenSlot() = default;
  ResolvedTokenSlot(ResolvedTokenSlot const&) = default;
  ResolvedTokenSlot(ResolvedTokenSlot&&) noexcept = default;
  ResolvedTokenSlot& operator=(ResolvedTokenSlot const&) = default;
  ResolvedTokenSlot& operator=(ResolvedTokenSlot&&) noexcept = default;

  bool is_resolved() const { return slot_.index() == 1; }

  TokenSlot const& token() const { return std::get<0>(slot_); }
  Symbol* symbol() const {
    return std::holds_alternative<Symbol*>(slot_) ? std::get<1>(slot_)
                                                  : nullptr;
  }

  sym_t type() const {
    switch (slot_.index()) {
      case 0:
        return std::get<0>(slot_).type();
      case 1:
        return std::get<1>(slot_)->type;
      default:
        throw std::runtime_error("Unexpected slot index");
    }
  }

  std::string_view name() const {
    switch (slot_.index()) {
      case 0:
        return std::get<0>(slot_).name();
      case 1:
        return std::get<1>(slot_)->name();
      default:
        throw std::runtime_error("Unexpected slot index");
    }
  }

  bool hasVal(int val) const {
    switch (slot_.index()) {
      case 0:
        return std::get<0>(slot_).hasVal(val);
      case 1:
        return std::get<1>(slot_)->hasVal(val);
      default:
        throw std::runtime_error("Unexpected slot index");
    }
  }

  int val() const {
    switch (slot_.index()) {
      case 0:
        return std::get<0>(slot_).val();
      case 1:
        return std::get<1>(slot_)->val();
      default:
        throw std::runtime_error("Unexpected slot index");
    }
  }

 private:
  explicit ResolvedTokenSlot(TokenSlot slot) : slot_(slot) {}
  explicit ResolvedTokenSlot(Symbol* sym) : slot_(sym) {}
  std::variant<TokenSlot, Symbol*> slot_;

  template <class Sink>
  friend void AbslStringify(Sink& sink, ResolvedTokenSlot const& token) {
    switch (token.slot_.index()) {
      case 0:
        absl::Format(&sink, "ResolvedTokenSlot(token: %v)", token.token());
        break;
      case 1:
        absl::Format(&sink, "ResolvedTokenSlot(symbol: %v)", *token.symbol());
        break;
      default:
        throw std::runtime_error("Unexpected slot index");
    }
  }
};

using RuntimeNumberOrString = std::variant<int, codegen::TextRef>;

[[nodiscard]] ResolvedTokenSlot LookupTok();
[[nodiscard]] ResolvedTokenSlot GetSymbol();
[[nodiscard]] std::optional<int> GetNumber(std::string_view);
[[nodiscard]] std::optional<RuntimeNumberOrString> GetNumberOrString(
    std::string_view);
[[nodiscard]] std::optional<TokenSlot> GetString(std::string_view);
[[nodiscard]] std::optional<TokenSlot> GetIdent();
bool GetDefineSymbol();
bool IsIdent(TokenSlot const& token);
bool IsUndefinedIdent(TokenSlot const& token);
keyword_t Keyword(TokenSlot const& token);
void GetKeyword(keyword_t);
bool IsVar(ResolvedTokenSlot const& token);
bool IsProc(ResolvedTokenSlot const& token);
bool IsObj(ResolvedTokenSlot const& token);
bool IsNumber(TokenSlot const& token);

extern int gSelectorIsVar;

#endif