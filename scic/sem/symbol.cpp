#include "scic/sem/symbol.hpp"

#include <utility>

namespace sem {

Symbol const* SymbolTable::AddSymbol(SymbolId id, Symbol::Value value) {
  auto [it, inserted] = symbols_.emplace(id, Symbol(id, std::move(value)));
  if (!inserted) {
    return nullptr;
  }

  it->second.value_mut() = value;
  return &it->second;
}

Symbol const* SymbolTable::FindSymbol(SymbolId::View id) const {
  auto it = symbols_.find(id);
  if (it == symbols_.end()) {
    return nullptr;
  }

  return &it->second;
}

Symbol const* SymbolTableStack::AddSymbol(SymbolId id, Symbol::Value value) {
  if (current_table_) {
    return current_table_->AddSymbol(id, std::move(value));
  }

  return nullptr;
}

Symbol const* SymbolTableStack::FindSymbol(SymbolId::View id) const {
  for (auto* stack = this; stack; stack = stack->prev_) {
    if (auto* symbol = stack->current_table_->FindSymbol(id)) {
      return symbol;
    }
  }

  return nullptr;
}

}  // namespace sem