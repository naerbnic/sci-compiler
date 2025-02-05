#ifndef SELECTOR_HPP
#define SELECTOR_HPP

#include <cstdint>
#include <string_view>

struct ANode;
class Symbol;

// Structure of a node in a class or object template.
struct Selector {
  Selector(Symbol* s = 0) : sym(s), val(0), an(0), tag(0) {}

  Symbol* sym;  // Pointer to symbol for this entry
  int val;      //	For a property, its initial value
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