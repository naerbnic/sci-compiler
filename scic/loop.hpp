#ifndef LOOP_HPP
#define LOOP_HPP

#include "scic/codegen/code_generator.hpp"

struct PNode;

void MakeWhile(FunctionBuilder* curList, PNode*);
void MakeRepeat(FunctionBuilder* curList, PNode*);
void MakeFor(FunctionBuilder* curList, PNode*);
void MakeBreak(FunctionBuilder* curList, PNode*);
void MakeBreakIf(FunctionBuilder* curList, PNode*);
void MakeContinue(FunctionBuilder* curList, PNode*);
void MakeContIf(FunctionBuilder* curList, PNode*);

#endif