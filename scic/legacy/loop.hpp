#ifndef LOOP_HPP
#define LOOP_HPP

#include "scic/codegen/code_generator.hpp"

struct PNode;

void MakeWhile(codegen::FunctionBuilder* curList, PNode*);
void MakeRepeat(codegen::FunctionBuilder* curList, PNode*);
void MakeFor(codegen::FunctionBuilder* curList, PNode*);
void MakeBreak(codegen::FunctionBuilder* curList, PNode*);
void MakeBreakIf(codegen::FunctionBuilder* curList, PNode*);
void MakeContinue(codegen::FunctionBuilder* curList, PNode*);
void MakeContIf(codegen::FunctionBuilder* curList, PNode*);

#endif