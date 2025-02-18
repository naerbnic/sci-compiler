// compile.hpp

#ifndef COMPILE_HPP
#define COMPILE_HPP

#include <cstdint>

#include "scic/alist.hpp"
#include "scic/anode_impls.hpp"
#include "scic/reference.hpp"

struct ANode;
class Symbol;
struct Object;
struct PNode;

void CompileProc(PNode* pn);
void CompileExpr(AOpList* curList, PNode* pn);
void MakeBranch(AOpList* curList, uint8_t type, ANLabel* target);
void MakeBranch(AOpList* curList, uint8_t type,
                ForwardReference<ANLabel*>* target);
void MakeObject(Object* theObj);
void MakeLabel(AOpList* curList, ForwardReference<ANLabel*>* ref);

#endif
