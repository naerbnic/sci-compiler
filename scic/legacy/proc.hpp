#ifndef PROC_HPP
#define PROC_HPP

#include <memory>

#include "scic/legacy/pnode.hpp"
#include "scic/legacy/symtypes.hpp"

void Procedure();
std::unique_ptr<PNode> CallDef(sym_t);

extern bool gInParmList;

#endif