//	asm.hpp
// 	definitions for assembly

#if !defined(ASM_HPP)
#define ASM_HPP

#if !defined(ALIST_HPP)
#include "alist.hpp"
#endif

void InitAsm();
void Assemble();

void Optimize();
void ShowOptimize();

struct ANTable;
struct ANWord;

extern int lastLineNum;
extern ANTable* dispTbl;
extern ANWord* numDispTblEntries;

#endif
