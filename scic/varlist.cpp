#include "scic/varlist.hpp"

void VarList::kill() {
  type = VAR_NONE;
  values.clear();
}