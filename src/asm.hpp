//	asm.hpp
// 	definitions for assembly

#if !defined(ASM_HPP)
#define ASM_HPP

#include "listing.hpp"

void InitAsm();
void Assemble(ListingFile* listFile);

struct ANTable;
struct ANWord;

extern int gLastLineNum;
extern ANTable* gDispTbl;
extern ANWord* gNumDispTblEntries;

#endif
