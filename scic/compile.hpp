// compile.hpp

#ifndef COMPILE_HPP
#define COMPILE_HPP

#include <cstdint>

#include "scic/alist.hpp"

struct PNode;
struct ANode;
class Symbol;
struct Object;

//	compile.cpp
void CompileProc(AList* curList, PNode* pn);
void CompileExpr(AOpList* curList, PNode* pn);
void MakeBranch(AOpList* curList, uint8_t type, ANode*, Symbol* target);
void MakeDispatch(int maxNum);
void MakeObject(Object* theObj);
void MakeText();
void MakeLabel(AOpList* curList, Symbol* sym);

//	loop.cpp
void MakeWhile(AOpList* curList, PNode*);
void MakeRepeat(AOpList* curList, PNode*);
void MakeFor(AOpList* curList, PNode*);
void MakeBreak(AOpList* curList, PNode*);
void MakeBreakIf(AOpList* curList, PNode*);
void MakeContinue(AOpList* curList, PNode*);
void MakeContIf(AOpList* curList, PNode*);

#endif
