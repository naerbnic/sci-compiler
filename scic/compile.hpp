// compile.hpp

#ifndef COMPILE_HPP
#define COMPILE_HPP

#include <cstdint>

#include "scic/alist.hpp"
#include "scic/anode.hpp"
#include "scic/reference.hpp"

struct ANode;
class Symbol;
struct Object;
struct PNode;

void CompileProc(AList* curList, PNode* pn);
void CompileExpr(AOpList* curList, PNode* pn);
void MakeBranch(AOpList* curList, uint8_t type, ANLabel* target);
void MakeBranch(AOpList* curList, uint8_t type,
                ForwardReference<ANLabel*>* target);
void MakeDispatch(int maxNum);
void MakeObject(Object* theObj);
void MakeText();
void MakeLabel(AOpList* curList, ForwardReference<ANLabel*>* ref);

#endif
