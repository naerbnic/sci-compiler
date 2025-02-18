#include "scic/compiler.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <variant>

#include "absl/strings/str_format.h"
#include "scic/alist.hpp"
#include "scic/anode.hpp"
#include "scic/anode_impls.hpp"
#include "scic/common.hpp"
#include "scic/config.hpp"
#include "scic/define.hpp"
#include "scic/fixup_list.hpp"
#include "scic/input.hpp"
#include "scic/listing.hpp"
#include "scic/output.hpp"
#include "scic/public.hpp"
#include "scic/sc.hpp"
#include "scic/symbol.hpp"
#include "scic/varlist.hpp"
#include "util/types/overload.hpp"

namespace {
class CompilerHeapContext : public HeapContext {
 public:
  CompilerHeapContext(Compiler* compiler) : compiler_(compiler) {}

  bool IsInHeap(ANode* node) const override {
    return compiler_->IsInHeap(node);
  }

 private:
  Compiler* compiler_;
};

class ANVars : public ANode
// The ANVars class is used to generate the block of variables for the
// module.
{
 public:
  ANVars(VarList* theVars) : theVars(theVars) {}

  size_t size() override { return 2 * (theVars->values.size() + 1); }

  void list(ListingFile* listFile) override {
    // FIXME: I don't know why we're saving/restoring the variable.
    std::size_t curOfs = *offset;

    listFile->Listing("\n\nVariables:");
    listFile->ListWord(curOfs, theVars->values.size());
    curOfs += 2;

    for (Var const& var : theVars->values) {
      if (std::holds_alternative<int>(*var.value)) {
        listFile->ListWord(curOfs, std::get<int>(*var.value));
      } else {
        ANText* text = std::get<ANText*>(*var.value);
        listFile->ListWord(curOfs, *text->offset);
      }
      curOfs += 2;
    }
    listFile->Listing("\n");
  }

  void collectFixups(FixupContext* fixup_ctxt) override {
    std::size_t relOfs = 2;
    for (Var const& var : theVars->values) {
      if (std::holds_alternative<ANText*>(*var.value)) {
        fixup_ctxt->AddRelFixup(this, relOfs);
      }
      relOfs += 2;
    }
  }

  void emit(OutputFile* out) override {
    out->WriteWord(theVars->values.size());

    for (Var const& var : theVars->values) {
      auto value = var.value.value_or(0);
      std::visit(
          util::Overload([&](int num) { out->WriteWord(num); },
                         [&](ANText* text) { out->WriteWord(*text->offset); }),
          value);
    }
  }

 protected:
  VarList* theVars;
};

class ANIntVar : public ANComputedWord {
 public:
  ANIntVar(int v) : v(v) {}

 protected:
  SCIWord value() const override { return v; }

 private:
  int v;
};

class ANStringVar : public ANComputedWord {
 public:
  ANStringVar(ANText* text) : text(text) {}

 protected:
  SCIWord value() const override { return *text->offset; }

 private:
  ANText* text;
};

void OptimizeHunk(ANode* anode) {
  if (!gConfig->noOptimize) {
    while (anode->optimize()) {
    }
  }

  // Make a first pass, resolving offsets and converting to byte offsets
  // where possible.
  anode->setOffset(0);

  // Continue resolving and converting to byte offsets until we've shrunk
  // the code as far as it will go.
  while (true) {
    if (!anode->tryShrink()) break;

    anode->setOffset(0);
  }
}

}  // namespace

Compiler::Compiler() {
  hunkList = std::make_unique<FixupList>();
  heapList = std::make_unique<FixupList>();
}

Compiler::~Compiler() = default;

void Compiler::InitAsm() {
  // Initialize the assembly list: dispose of any old list, then add nodes
  // for the number of local variables.

  localVars.kill();

  hunkList = std::make_unique<FixupList>();
  heapList = std::make_unique<FixupList>();

  //	setup the debugging info
  lastLineNum = 0;

  auto* hunkBody = hunkList->getBody();

  // space for addr of heap component of resource
  hunkBody->newNode<ANWord>();

  // space to indicate whether script has far text (dummy)
  hunkBody->newNode<ANWord>();

  auto* numDispTblEntries = hunkBody->newNode<ANCountWord>(nullptr);
  dispTbl = hunkBody->newNode<ANTable>("dispatch table");
  numDispTblEntries->target = dispTbl->getList();
  objDictList = hunkBody->newNode<ANTable>("object dict list")->getList();
  codeList = hunkBody->newNode<ANTable>("code list")->getList();

  auto* heapBody = heapList->getBody();
  heapBody->newNode<ANVars>(&localVars);

  objPropList = heapBody->newNode<ANTable>("object properties")->getList();
  // The object section terminator.
  heapBody->newNode<ANWord>(0);

  textList = heapBody->newNode<ANTable>("text table")->getList();
}

void Compiler::Assemble(ListingFile* listFile) {
  // Set the offsets in the object list.
  heapList->setOffset(0);

  // Optimize the code, setting all the offsets.
  OptimizeHunk(hunkList->getRoot());

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

  {
    CompilerHeapContext heapContext(this);
    heapList->emit(&heapContext, obj_files.heap.get());
    hunkList->emit(&heapContext, obj_files.hunk.get());
  }

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

  heapList = nullptr;
  hunkList = nullptr;
}

void Compiler::MakeDispatch(PublicList const& publicList) {
  // Compile the dispatch table which goes at the start of this script.

  // Now cycle through the publicly declared procedures/objects,
  // creating asmNodes for a table of their offsets.
  std::uint32_t maxEntry = 0;
  for (auto const& pub : publicList) {
    maxEntry = std::max(pub->entry, maxEntry);
  }
  for (int i = 0; i <= maxEntry; ++i) {
    ANDispatch* an = dispTbl->getList()->newNode<ANDispatch>();
    auto* sym = FindPublic(publicList, i);
    if (sym) {
      an->name = std::string(sym->name());
      sym->forwardRef.RegisterCallback(
          [an](ANode* target) { an->target = target; });
    }
  }
}

bool Compiler::IsInHeap(ANode* node) { return heapList->contains(node); }

ANText* Compiler::AddTextNode(std::string_view text) {
  auto it = textNodes.find(text);
  if (it != textNodes.end()) return it->second;
  auto* textNode = textList->newNode<ANText>(std::string(text));

  textNodes.emplace(std::string(text), textNode);
  return textNode;
}

std::size_t Compiler::NumVars() const { return localVars.values.size(); }

bool Compiler::SetTextVar(std::size_t varNum, ANText* text) {
  if (localVars.values.size() <= varNum) {
    localVars.values.resize(varNum + 1);
  }

  Var* vp = &localVars.values[varNum];
  if (vp->value) {
    return false;
  }

  vp->value = text;
  return true;
}

bool Compiler::SetIntVar(std::size_t varNum, int value) {
  if (localVars.values.size() <= varNum) {
    localVars.values.resize(varNum + 1);
  }

  Var* vp = &localVars.values[varNum];
  if (vp->value) {
    return false;
  }

  vp->value = value;
  return true;
}
