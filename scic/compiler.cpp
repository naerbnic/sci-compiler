#include "scic/compiler.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"
#include "scic/alist.hpp"
#include "scic/anode.hpp"
#include "scic/anode_impls.hpp"
#include "scic/common.hpp"
#include "scic/config.hpp"
#include "scic/fixup_list.hpp"
#include "scic/input.hpp"
#include "scic/listing.hpp"
#include "scic/output.hpp"
#include "scic/reference.hpp"
#include "scic/varlist.hpp"
#include "util/types/overload.hpp"

namespace {
class CompilerHeapContext : public HeapContext {
 public:
  CompilerHeapContext(Compiler* compiler) : compiler_(compiler) {}

  bool IsInHeap(ANode const* node) const override {
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

  size_t size() const override { return 2 * (theVars->values.size() + 1); }

  void list(ListingFile* listFile) const override {
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

  void collectFixups(FixupContext* fixup_ctxt) const override {
    std::size_t relOfs = 2;
    for (Var const& var : theVars->values) {
      if (std::holds_alternative<ANText*>(*var.value)) {
        fixup_ctxt->AddRelFixup(this, relOfs);
      }
      relOfs += 2;
    }
  }

  void emit(OutputFile* out) const override {
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

class ANDispTable : public ANode {
 public:
  void AddPublic(std::string name, std::size_t index,
                 ForwardReference<ANode*>* target) {
    while (dispatches_.size() <= index) {
      dispatches_.push_back(std::make_unique<ANDispatch>());
    }
    auto* pub = dispatches_[index].get();
    pub->name = std::move(name);
    target->RegisterCallback([pub](ANode* target) { pub->target = target; });
  };

  std::size_t size() const override { return 2 + dispatches_.size() * 2; }

  void list(ListingFile* listFile) const override {
    listFile->Listing("\n\nDispatch Table:");
    listFile->ListWord(*offset, dispatches_.size());
    for (auto const& dispatch : dispatches_) {
      dispatch->list(listFile);
    }
  }

  void collectFixups(FixupContext* fixup_ctxt) const override {
    for (auto const& dispatch : dispatches_) {
      dispatch->collectFixups(fixup_ctxt);
    }
  }

  void emit(OutputFile* out) const override {
    out->WriteWord(dispatches_.size());
    for (auto const& dispatch : dispatches_) {
      dispatch->emit(out);
    }
  }

  bool contains(ANode const* node) const override {
    for (auto const& dispatch : dispatches_) {
      if (dispatch->contains(node)) {
        return true;
      }
    }
    return false;
  }

  bool optimize() override {
    bool changed = false;
    for (auto& dispatch : dispatches_) {
      if (dispatch->optimize()) {
        changed = true;
      }
    }
    return changed;
  }

 private:
  std::vector<std::unique_ptr<ANDispatch>> dispatches_;
};

ANode* ObjectCodegen::GetObjNode() const { return propListMarker_; }

void ObjectCodegen::AppendProperty(std::string name, std::uint16_t selectorNum,
                                   LiteralValue value) {
  std::visit(util::Overload(
                 [&](int num) {
                   props_->getList()->newNode<ANIntProp>(std::move(name), num);
                 },
                 [&](ANText* text) {
                   props_->getList()->newNode<ANOfsProp>(std::move(name), text);
                 }),
             value);
  AppendPropDict(selectorNum);
}

void ObjectCodegen::AppendPropTableProperty(std::string name,
                                            std::uint16_t selectorNum) {
  props_->getList()->newNode<ANOfsProp>(std::string(name), propDict_);
  AppendPropDict(selectorNum);
}

void ObjectCodegen::AppendMethodTableProperty(std::string name,
                                              std::uint16_t selectorNum) {
  props_->getList()->newNode<ANOfsProp>(std::string(name), methDictStart_);
  AppendPropDict(selectorNum);
}

void ObjectCodegen::AppendMethod(std::string name, std::uint16_t selectorNum,
                                 ANCodeBlk* code) {
  auto* entry = methDict_->getList()->newNode<ANComposite<ANode>>();
  entry->getList()->newNode<ANWord>(selectorNum);
  entry->getList()->newNode<ANMethod>(std::move(name), code);
}

std::unique_ptr<ObjectCodegen> ObjectCodegen::Create(Compiler* compiler,
                                                     bool isObj,
                                                     std::string name) {
  // Allocate tables in the correct places in the heap/hunk.
  //
  // This does not actually allocate any space, but it does create growable
  // tables.
  ANObject* propListMarker = compiler->objPropList->newNode<ANObject>(name);
  ANTable* props = compiler->objPropList->newNode<ANTable>("properties");

  ANObject* objDictMarker = compiler->objDictList->newNode<ANObject>(name);
  ANObjTable* propDict =
      compiler->objDictList->newNode<ANObjTable>("property dictionary");
  // The size of the method dictionary.
  ANCountWord* methDictSize =
      compiler->objDictList->newNode<ANCountWord>(nullptr);
  ANObjTable* methDict =
      compiler->objDictList->newNode<ANObjTable>("method dictionary");
  methDictSize->target = methDict->getList();
  return absl::WrapUnique(new ObjectCodegen(isObj, name, propListMarker, props,
                                            objDictMarker, propDict,
                                            methDictSize, methDict));
}

ObjectCodegen::ObjectCodegen(bool isObj, std::string name,
                             ANObject* propListMarker, ANTable* props,
                             ANObject* objDictMarker, ANObjTable* propDict,
                             ANode* methDictStart, ANObjTable* methDict)
    : isObj_(isObj),
      name_(name),
      propListMarker_(propListMarker),
      props_(props),
      objDictMarker_(objDictMarker),
      propDict_(propDict),
      methDictStart_(methDictStart),
      methDict_(methDict) {}

void ObjectCodegen::AppendPropDict(std::uint16_t selectorNum) {
  if (!isObj_) {
    propDict_->getList()->newNode<ANWord>(selectorNum);
  }
}

// -----------------

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

  auto* hunkBody = hunkList->getBody();

  // space for addr of heap component of resource
  hunkBody->newNode<ANWord>();

  // space to indicate whether script has far text (dummy)
  hunkBody->newNode<ANWord>();

  dispTable = hunkBody->newNode<ANDispTable>();
  objDictList = hunkBody->newNode<ANTable>("object dict list")->getList();
  codeList = hunkBody->newNode<ANTable>("code list")->getList();

  auto* heapBody = heapList->getBody();
  heapBody->newNode<ANVars>(&localVars);

  objPropList = heapBody->newNode<ANTable>("object properties")->getList();
  // The object section terminator.
  heapBody->newNode<ANWord>(0);

  textList = heapBody->newNode<ANTable>("text table")->getList();
}

void Compiler::Assemble(uint16_t scriptNum, ListingFile* listFile) {
  // Set the offsets in the object list.
  heapList->setOffset(0);

  // Optimize the code, setting all the offsets.
  OptimizeHunk(hunkList->getRoot());

  // Reset the offsets in the object list to get the current code
  // offsets.
  heapList->setOffset(0);

  ObjFiles obj_files = OpenObjFiles(scriptNum);

  const std::size_t MAX_INFO_FILE_NAME = 1024;

  char infoFileName[MAX_INFO_FILE_NAME];
  if (snprintf(infoFileName, 1024, "%d.inf", (unsigned short)scriptNum) >=
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

void Compiler::AddPublic(std::string name, std::size_t index,
                         ForwardReference<ANode*>* target) {
  dispTable->AddPublic(std::move(name), index, target);
}

bool Compiler::IsInHeap(ANode const* node) { return heapList->contains(node); }

ANText* Compiler::AddTextNode(std::string_view text) {
  auto it = textNodes.find(text);
  if (it != textNodes.end()) return it->second;
  auto* textNode = textList->newNode<ANText>(std::string(text));

  textNodes.emplace(std::string(text), textNode);
  return textNode;
}

std::size_t Compiler::NumVars() const { return localVars.values.size(); }

bool Compiler::SetVar(std::size_t varNum, LiteralValue value) {
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

std::unique_ptr<ObjectCodegen> Compiler::CreateObject(std::string name) {
  return ObjectCodegen::Create(this, true, name);
}

std::unique_ptr<ObjectCodegen> Compiler::CreateClass(std::string name) {
  return ObjectCodegen::Create(this, false, name);
}

ANCodeBlk* Compiler::CreateProcedure(std::string name) {
  return codeList->newNode<ANProcCode>(std::move(name));
}

ANCodeBlk* Compiler::CreateMethod(std::string objName, std::string name) {
  return codeList->newNode<ANMethCode>(std::move(name), std::move(objName));
}