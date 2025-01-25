// compile.hpp

#ifndef COMPILE_HPP
#define COMPILE_HPP

#include <cstdint>

struct PNode;
struct ANode;
class Symbol;
struct Object;

//	compile.cpp
void Compile(PNode* pn);
void MakeBranch(uint8_t type, ANode*, Symbol* target);
void MakeDispatch(int maxNum);
void MakeObject(Object* theObj);
void MakeText();
void MakeLabel(Symbol* sym);

//	loop.cpp
void MakeWhile(PNode*);
void MakeRepeat(PNode*);
void MakeFor(PNode*);
void MakeBreak(PNode*);
void MakeBreakIf(PNode*);
void MakeContinue(PNode*);
void MakeContIf(PNode*);

#endif
