// compile.hpp

#ifndef COMPILE_HPP
#define COMPILE_HPP

#include <cstdint>

#include "scic/codegen/anode_impls.hpp"
#include "scic/codegen/code_generator.hpp"
#include "util/types/forward_ref.hpp"

struct ANode;
class Symbol;
struct Object;
struct PNode;

void CompileProc(PNode* pn);
void CompileExpr(FunctionBuilder* curList, PNode* pn);
void MakeBranch(FunctionBuilder* curList, uint8_t type, ANLabel* target);
void MakeBranch(FunctionBuilder* curList, uint8_t type,
                ForwardRef<ANLabel*>* target);
void MakeObject(Object* theObj);
void MakeLabel(FunctionBuilder* curList, ForwardRef<ANLabel*>* ref);

#endif
