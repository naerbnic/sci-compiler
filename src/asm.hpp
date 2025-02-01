//	asm.hpp
// 	definitions for assembly

#if !defined(ASM_HPP)
#define ASM_HPP

#include "listing.hpp"

void InitAsm();
void Assemble(ListingFile* listFile);

void Optimize();
void ShowOptimize();

struct ANTable;
struct ANWord;

extern int lastLineNum;
extern ANTable* dispTbl;
extern ANWord* numDispTblEntries;

#endif
