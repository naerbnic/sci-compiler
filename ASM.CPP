//	asm.cpp
// 	assemble an object code list

#include "sol.hpp"

#include	"sc.hpp"

#include	"anode.hpp"
#include	"asm.hpp"
#include	"define.hpp"
#include	"input.hpp"
#include	"listing.hpp"
#include	"object.hpp"
#include	"output.hpp"
#include	"symbol.hpp"
#include	"text.hpp"

Bool			addNodesToList;
ANTable*		dispTbl;
int			lastLineNum;
ANWord*		numDispTblEntries;

void
InitAsm()
{
	// Initialize the assembly list: dispose of any old list, then add nodes
	// for the number of local variables.

	localVars.kill();

	addNodesToList = True;
	textStart = 0;

	sc->heapList->clear();
	sc->hunkList->clear();

	//	setup the debugging info
	lastLineNum = 0;

	// space for addr of heap component of resource
	New ANWord(sc->hunkList);

	// space to indicate whether script has far text (dummy)
	New ANWord(sc->hunkList);

	numDispTblEntries = New ANWord;
	dispTbl = New ANTable("dispatch table");
	dispTbl->finish();

	codeStart = 0;
	curList = sc->hunkList;
}

void
Assemble()
{
	// Assemble the list pointed to by asmHead.

	New ANVars(script ? localVars : globalVars);

	// Set the offsets in the object list.
	sc->heapList->setOffset(0);

	// Optimize the code, setting all the offsets.
	addNodesToList = False;
	sc->hunkList->optimize();
	addNodesToList = True;

	// Reset the offsets in the object list to get the current code
	// offsets.
	sc->heapList->setOffset(0);

	OutputFile* heapOut;
	OutputFile* hunkOut;
	OpenObjFiles(&heapOut, &hunkOut);

	char infoFileName[1024];
	sprintf ( infoFileName, "%d.inf", (unsigned short)script );
	FILE *infoFile = fopen ( infoFileName, "wb" );
	fprintf ( infoFile, "%s\n", curSourceFile->fileName );
	fclose ( infoFile );

	// Now generate object code.
	Listing("----------------------\n"
			  "-------- Heap --------\n"
			  "----------------------\n"
	);
	sc->heapList->emit(heapOut);
	Listing("\n\n\n\n"
			  "----------------------\n"
			  "-------- Hunk --------\n"
			  "----------------------\n"
	);
	sc->hunkList->emit(hunkOut);

	delete heapOut;
	delete hunkOut;

	sc->heapList->clear();
	sc->hunkList->clear();
}

