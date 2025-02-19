#ifndef VARLIST_HPP
#define VARLIST_HPP

#include <optional>
#include <vector>

#include "scic/codegen/anode_impls.hpp"
#include "util/types/choice.hpp"

struct Var {
  // Variable definition.
  Var() {}

  std::optional<util::Choice<int, ANText*>> value;
};

struct VarList {
  // Variable block definition.
  VarList() {}

  void kill();

  std::vector<Var> values;  // pointer to block of initial values
};

#endif