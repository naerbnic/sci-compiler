// compile.hpp

#ifndef COMPILE_HPP
#define COMPILE_HPP

#include <cstdint>

#include "alist.hpp"

struct PNode;
struct ANode;
class Symbol;
struct Object;

//	compile.cpp
void CompileProc(AList* curList, PNode* pn);
void CompileExpr(AList* curList, PNode* pn);
void MakeBranch(AList* curList, uint8_t type, ANode*, Symbol* target);
void MakeDispatch(int maxNum);
void MakeObject(Object* theObj);
void MakeText();
void MakeLabel(AList* curList, Symbol* sym);

//	loop.cpp
void MakeWhile(AList* curList, PNode*);
void MakeRepeat(AList* curList, PNode*);
void MakeFor(AList* curList, PNode*);
void MakeBreak(AList* curList, PNode*);
void MakeBreakIf(AList* curList, PNode*);
void MakeContinue(AList* curList, PNode*);
void MakeContIf(AList* curList, PNode*);

#endif
