#ifndef VARLIST_HPP
#define VARLIST_HPP

#include <optional>
#include <variant>
#include <vector>

#include "scic/anode.hpp"

struct Var {
  // Variable definition.
  Var() : value(0) {}

  std::optional<std::variant<int, ANText*>> value;
};

struct VarList {
  // Variable block definition.
  VarList() {}

  void kill();

  std::vector<Var> values;  // pointer to block of initial values
};

#endif