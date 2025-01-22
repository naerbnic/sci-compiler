//	asm.cpp
// 	assemble an object code list

#include "asm.hpp"

#include <cstdlib>

#include "absl/strings/str_format.h"
#include "anode.hpp"
#include "define.hpp"
#include "input.hpp"
#include "listing.hpp"
#include "object.hpp"
#include "output.hpp"
#include "sc.hpp"
#include "sol.hpp"
#include "symbol.hpp"
#include "text.hpp"

bool addNodesToList;
ANTable* dispTbl;
int lastLineNum;
ANWord* numDispTblEntries;

void InitAsm() {
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
  sc->hunkList->newNode<ANWord>();

  // space to indicate whether script has far text (dummy)
  sc->hunkList->newNode<ANWord>();

  if (curList) {
    numDispTblEntries = curList->newNode<ANWord>();
    dispTbl = curList->newNodeBefore<ANTable>(nullptr, "dispatch table");
  } else {
    numDispTblEntries = new ANWord();
    dispTbl = new ANTable("dispatch table");
  }
  dispTbl->finish();

  codeStart = 0;
  curList = sc->hunkList;
}

void Assemble() {
  // Assemble the list pointed to by asmHead.

  auto vars = std::make_unique<ANVars>(script ? localVars : globalVars);
  sc->heapList->incFixups(vars->numFixups());
  sc->heapList->addAfter(sc->heapList->first(), std::move(vars));

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

  const ssize_t MAX_INFO_FILE_NAME = 1024;

  char infoFileName[MAX_INFO_FILE_NAME];
  if (snprintf(infoFileName, 1024, "%d.inf", (unsigned short)script) >=
      MAX_INFO_FILE_NAME) {
    fprintf(stderr, "Error: info file name too long\n");
    exit(1);
  }
  FILE* infoFile = fopen(infoFileName, "wb");
  absl::FPrintF(infoFile, "%s\n", curSourceFile->fileName);
  fclose(infoFile);

  // Now generate object code.
  Listing(
      "----------------------\n"
      "-------- Heap --------\n"
      "----------------------\n");
  sc->heapList->emit(heapOut);
  Listing(
      "\n\n\n\n"
      "----------------------\n"
      "-------- Hunk --------\n"
      "----------------------\n");
  sc->hunkList->emit(hunkOut);

  delete heapOut;
  delete hunkOut;

  sc->heapList->clear();
  sc->hunkList->clear();
}
