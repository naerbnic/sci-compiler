// compile.hpp

#if !defined(COMPILE_HPP)
#define COMPILE_HPP

class PNode;
class ANode;
class Symbol;
class Object;

//	compile.cpp
void	CompileCode(PNode* pn);
void	Compile(PNode* pn);
void	MakeBranch(ubyte type, ANode*, Symbol* target);
void	MakeDispatch(int maxNum);
void	MakeObject(Object* theObj);
void	MakeText();
void	MakeLabel(Symbol* sym);

//	loop.cpp
void	MakeWhile(PNode*);
void	MakeRepeat(PNode*);
void	MakeFor(PNode*);
void	MakeBreak(PNode*);
void	MakeBreakIf(PNode*);
void	MakeContinue(PNode*);
void	MakeContIf(PNode*);

#endif

