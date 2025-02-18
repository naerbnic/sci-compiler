#ifndef SELECTOR_HPP
#define SELECTOR_HPP

#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

#include "scic/codegen/anode_impls.hpp"

struct ANode;
class Symbol;

// Structure of a node in a class or object template.
struct Selector {
  Selector(Symbol* s = 0) : sym(s), an(0), tag(0) {}

  Symbol* sym;  // Pointer to symbol for this entry
  //	For a property, its initial value
  std::optional<std::variant<int, ANText*>> val;
  union {
    int ofs;    // Offset of property in template
    ANode* an;  // Pointer to code for a local method
  };
  uint32_t tag;
};

Symbol* GetSelector(Symbol*);
void InitSelectors();
Symbol* InstallSelector(std::string_view name, int value);
int NewSelectorNum();

extern int gMaxSelector;

#endif