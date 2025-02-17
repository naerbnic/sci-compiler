//	asm.cpp
// 	assemble an object code list

#include "scic/asm.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <utility>

#include "absl/strings/str_format.h"
#include "scic/anode.hpp"
#include "scic/compiler.hpp"
#include "scic/fixup_list.hpp"
#include "scic/global_compiler.hpp"
#include "scic/input.hpp"
#include "scic/listing.hpp"
#include "scic/output.hpp"
#include "scic/parse_context.hpp"
#include "scic/sc.hpp"

ANTable* gDispTbl;
int gLastLineNum;
ANWord* gNumDispTblEntries;

void InitAsm() {
  // Initialize the assembly list: dispose of any old list, then add nodes
  // for the number of local variables.

  gLocalVars.kill();

  gTextStart = 0;

  gSc->heapList->clear();
  gSc->hunkList->clear();

  //	setup the debugging info
  gLastLineNum = 0;

  // space for addr of heap component of resource
  gSc->hunkList->newNode<ANWord>();

  // space to indicate whether script has far text (dummy)
  gSc->hunkList->newNode<ANWord>();

  gNumDispTblEntries = gSc->hunkList->newNode<ANWord>();
  gDispTbl = gSc->hunkList->newNode<ANTable>("dispatch table");

  gCodeStart = 0;
}

void Assemble(ListingFile* listFile) {
  // Assemble the list pointed to by asmHead.

  auto vars = std::make_unique<ANVars>(gScript ? gLocalVars : gGlobalVars);
  gSc->heapList->addAfter(gSc->heapList->front(), std::move(vars));

  // Set the offsets in the object list.
  gSc->heapList->setOffset(0);

  // Optimize the code, setting all the offsets.
  gSc->hunkList->optimize();

  // Reset the offsets in the object list to get the current code
  // offsets.
  gSc->heapList->setOffset(0);

  ObjFiles obj_files = OpenObjFiles(gScript);

  const std::size_t MAX_INFO_FILE_NAME = 1024;

  char infoFileName[MAX_INFO_FILE_NAME];
  if (snprintf(infoFileName, 1024, "%d.inf", (unsigned short)gScript) >=
      (int)MAX_INFO_FILE_NAME) {
    fprintf(stderr, "Error: info file name too long\n");
    exit(1);
  }
  FILE* infoFile = fopen(infoFileName, "wb");
  absl::FPrintF(infoFile, "%s\n", gInputState.GetTopLevelFileName());
  fclose(infoFile);

  gSc->heapList->emit(gSc.get(), obj_files.heap.get());
  gSc->hunkList->emit(gSc.get(), obj_files.hunk.get());

  // Now generate object code.
  listFile->Listing(
      "----------------------\n"
      "-------- Heap --------\n"
      "----------------------\n");
  gSc->heapList->list(listFile);
  listFile->Listing(
      "\n\n\n\n"
      "----------------------\n"
      "-------- Hunk --------\n"
      "----------------------\n");
  gSc->hunkList->list(listFile);

  gSc->heapList->clear();
  gSc->hunkList->clear();
}
