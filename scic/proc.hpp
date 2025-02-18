#ifndef PROC_HPP
#define PROC_HPP

#include <memory>

#include "scic/pnode.hpp"
#include "scic/symtypes.hpp"

void Procedure();
std::unique_ptr<PNode> CallDef(sym_t);

extern bool gInParmList;

#endif