#ifndef SEM_SYMBOL_HPP
#define SEM_SYMBOL_HPP

#include <functional>
#include <map>
#include <string_view>
#include <utility>

#include "util/types/choice.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/strong_types.hpp"

namespace sem {
struct RefStrTag {
  using Value = util::RefStr;
  static constexpr bool is_const = true;

  using View = std::string_view;
  using ViewStorage = std::string_view;

  static ViewStorage ValueToStorage(Value const& value) { return value; }
  static View StorageToView(ViewStorage const& storage) { return storage; }
};

struct SymbolIdTag : RefStrTag {};
using SymbolId = util::StrongValue<SymbolIdTag>;

struct SelectorIdTag : RefStrTag {};
using SelectorId = util::StrongValue<SelectorIdTag>;

struct IntBaseTag {
  using Value = int;
  static constexpr bool is_const = true;
};

struct GlobalIndexTag : IntBaseTag {};

// An index into the global table of symbols.
using GlobalIndex = util::StrongValue<GlobalIndexTag>;

struct LocalIndexTag : IntBaseTag {};

using LocalIndex = util::StrongValue<LocalIndexTag>;

struct ModuleIdTag : IntBaseTag {};
// The id of a module, a.k.a. the script number.
using ModuleId = util::StrongValue<ModuleIdTag>;

constexpr ModuleId kKernelModuleId = ModuleId::Create(-1);

struct PublicIndexTag : IntBaseTag {};
using PublicIndex = util::StrongValue<PublicIndexTag>;

struct ClassSpeciesTag : IntBaseTag {};
using ClassSpecies = util::StrongValue<ClassSpeciesTag>;

class Symbol {
 public:
  // These symbol value types are derived from the original code, and the
  // various calls to the SymTbls::install* methods.

  //  A global defined variable.
  struct Global {
    // The global index of the variable.
    GlobalIndex index;
  };

  // A local variable.
  struct ModuleVar {
    LocalIndex index;
    int array_size;
  };

  // An external symbol reference.
  struct Extern {
    // The module that the symbol is defined in.
    ModuleId module_id;
    // The index of the symbol in the module's public table.
    PublicIndex public_index;
  };

  struct Object {
    // Original code has a reference to an Object, but we will put it directly
    // in the symbol for now.
    ClassSpecies parent_class;
  };

  struct Procedure {};

  class Value : public util::ChoiceBase<Value, Global, Extern> {};

  Symbol(SymbolId id, Value value) : id_(id), value_(std::move(value)) {}

  SymbolId::View id() const { return id_; }
  Value const& value() const { return value_; }
  Value& value_mut() { return value_; }

 private:
  SymbolId id_;
  Value value_;
};

class SymbolTable {
 public:
  SymbolTable() = default;

  Symbol const* AddSymbol(SymbolId id, Symbol::Value value);

  Symbol const* FindSymbol(SymbolId::View id) const;

 private:
  std::map<SymbolId, Symbol, std::less<>> symbols_;
};

// A stack of symbol tables. This is intended to put on the stack in a recursive
// function, using RAII to manage the stack.
class SymbolTableStack {
 public:
  // Create the root of a symbol table stack.
  SymbolTableStack() : current_table_(nullptr), prev_(nullptr) {}
  SymbolTableStack(SymbolTable* current_table)
      : current_table_(current_table), prev_(nullptr) {}

  SymbolTableStack(SymbolTable* current_table, SymbolTableStack* prev)
      : current_table_(current_table), prev_(prev) {}

  // Add a symbol to the topmost table.
  Symbol const* AddSymbol(SymbolId id, Symbol::Value value);

  // Search the stack for a symbol.
  Symbol const* FindSymbol(SymbolId::View id) const;

 private:
  SymbolTable* current_table_;
  SymbolTableStack* prev_;
};

}  // namespace sem

#endif