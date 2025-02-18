#ifndef LOOP_HPP
#define LOOP_HPP

#include "scic/codegen/alist.hpp"

struct PNode;

void MakeWhile(AOpList* curList, PNode*);
void MakeRepeat(AOpList* curList, PNode*);
void MakeFor(AOpList* curList, PNode*);
void MakeBreak(AOpList* curList, PNode*);
void MakeBreakIf(AOpList* curList, PNode*);
void MakeContinue(AOpList* curList, PNode*);
void MakeContIf(AOpList* curList, PNode*);

#endif