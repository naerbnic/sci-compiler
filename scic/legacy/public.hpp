#ifndef PUBLIC_HPP
#define PUBLIC_HPP

#include <cstdint>
#include <deque>
#include <memory>

#include "scic/legacy/selector.hpp"

struct Public {
  // Node for public/export definition lists.

  Public(Symbol* s = 0) : sym(s), script(0), entry(0) {}

  Symbol* sym;     // pointer to the relevant symbol
  int script;      // script number
  uint32_t entry;  // index in dispatch table
};

using PublicList = std::deque<std::unique_ptr<Public>>;

#endif