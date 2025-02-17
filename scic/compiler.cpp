#include "scic/compiler.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <utility>

#include "absl/strings/str_format.h"
#include "scic/alist.hpp"
#include "scic/anode.hpp"
#include "scic/fixup_list.hpp"
#include "scic/input.hpp"
#include "scic/listing.hpp"
#include "scic/output.hpp"
#include "scic/parse_context.hpp"
#include "scic/sc.hpp"

Compiler::Compiler() {
  hunkList = std::make_unique<CodeList>();
  heapList = std::make_unique<FixupList>();
}

Compiler::~Compiler() = default;

bool Compiler::HeapHasNode(ANode* node) const {
  return heapList->contains(node);
}

void Compiler::AddFixup(std::size_t ofs) { heapList->addFixup(ofs); }

ANTable* gDispTbl;
int gLastLineNum;
ANWord* gNumDispTblEntries;

void Compiler::InitAsm() {
  // Initialize the assembly list: dispose of any old list, then add nodes
  // for the number of local variables.

  localVars.kill();

  gTextStart = 0;

  heapList->clear();
  hunkList->clear();

  //	setup the debugging info
  lastLineNum = 0;

  // space for addr of heap component of resource
  hunkList->newNode<ANWord>();

  // space to indicate whether script has far text (dummy)
  hunkList->newNode<ANWord>();

  gNumDispTblEntries = hunkList->newNode<ANWord>();
  gDispTbl = hunkList->newNode<ANTable>("dispatch table");

  gCodeStart = 0;
}

void Compiler::Assemble(ListingFile* listFile) {
  // Assemble the list pointed to by asmHead.

  auto vars = std::make_unique<ANVars>(gScript ? gLocalVars : gGlobalVars);
  heapList->addAfter(heapList->front(), std::move(vars));

  // Set the offsets in the object list.
  heapList->setOffset(0);

  // Optimize the code, setting all the offsets.
  hunkList->optimize();

  // Reset the offsets in the object list to get the current code
  // offsets.
  heapList->setOffset(0);

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

  heapList->emit(this, obj_files.heap.get());
  hunkList->emit(this, obj_files.hunk.get());

  // Now generate object code.
  listFile->Listing(
      "----------------------\n"
      "-------- Heap --------\n"
      "----------------------\n");
  heapList->list(listFile);
  listFile->Listing(
      "\n\n\n\n"
      "----------------------\n"
      "-------- Hunk --------\n"
      "----------------------\n");
  hunkList->list(listFile);

  heapList->clear();
  hunkList->clear();
}