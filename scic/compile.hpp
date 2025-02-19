// compile.hpp

#ifndef COMPILE_HPP
#define COMPILE_HPP

#include "scic/codegen/code_generator.hpp"

struct Object;
struct PNode;

void CompileProc(PNode* pn);
void CompileExpr(FunctionBuilder* curList, PNode* pn);
void MakeObject(Object* theObj);

#endif
