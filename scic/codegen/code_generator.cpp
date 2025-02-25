#include "scic/codegen/code_generator.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"
#include "scic/codegen/alist.hpp"
#include "scic/codegen/anode.hpp"
#include "scic/codegen/anode_impls.hpp"
#include "scic/codegen/common.hpp"
#include "scic/codegen/fixup_list.hpp"
#include "scic/codegen/target.hpp"
#include "scic/listing.hpp"
#include "scic/opcodes.hpp"
#include "scic/output.hpp"
#include "util/types/choice.hpp"
#include "util/types/forward_ref.hpp"

namespace codegen {

class Var {
 public:
  std::optional<util::Choice<int, TextRef>> value;
};

class CompilerHeapContext : public HeapContext {
 public:
  CompilerHeapContext(CodeGenerator* compiler) : compiler_(compiler) {}

  bool IsInHeap(ANode const* node) const override {
    return compiler_->IsInHeap(node);
  }

 private:
  CodeGenerator* compiler_;
};

class ANVars : public ANode
// The ANVars class is used to generate the block of variables for the
// module.
{
 public:
  ANVars(std::vector<Var> const* theVars) : theVars(theVars) {}

  size_t size() const override { return 2 * (theVars->size() + 1); }

  void list(ListingFile* listFile) const override {
    // FIXME: I don't know why we're saving/restoring the variable.
    std::size_t curOfs = *offset;

    listFile->Listing("\n\nVariables:");
    listFile->ListWord(curOfs, theVars->size());
    curOfs += 2;

    for (Var const& var : *theVars) {
      var.value->visit([&](int num) { listFile->ListWord(curOfs, num); },
                       [&](TextRef text) {
                         listFile->ListWord(curOfs, *text.ref_->offset);
                       });
      curOfs += 2;
    }
    listFile->Listing("\n");
  }

  void collectFixups(FixupContext* fixup_ctxt) const override {
    std::size_t relOfs = 2;
    for (Var const& var : *theVars) {
      if (var.value->has<TextRef>()) {
        fixup_ctxt->AddRelFixup(this, relOfs);
      }
      relOfs += 2;
    }
  }

  void emit(OutputFile* out) const override {
    out->WriteWord(theVars->size());

    for (Var const& var : *theVars) {
      auto value = var.value.value_or(0);
      value.visit([&](int num) { out->WriteWord(num); },
                  [&](TextRef text) { out->WriteWord(*text.ref_->offset); });
    }
  }

 protected:
  std::vector<Var> const* theVars;
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

void OptimizeHunk(Optimization opt, ANode* anode) {
  if (opt == Optimization::OPTIMIZE) {
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

uint8_t GetBinOpValue(FunctionBuilder::BinOp op) {
  switch (op) {
    case FunctionBuilder::ADD:
      return op_add;
    case FunctionBuilder::SUB:
      return op_sub;
    case FunctionBuilder::MUL:
      return op_mul;
    case FunctionBuilder::DIV:
      return op_div;
    case FunctionBuilder::SHL:
      return op_shl;
    case FunctionBuilder::SHR:
      return op_shr;
    case FunctionBuilder::MOD:
      return op_mod;
    case FunctionBuilder::AND:
      return op_and;
    case FunctionBuilder::OR:
      return op_or;
    case FunctionBuilder::XOR:
      return op_xor;
    case FunctionBuilder::GT:
      return op_gt;
    case FunctionBuilder::GE:
      return op_ge;
    case FunctionBuilder::LT:
      return op_lt;
    case FunctionBuilder::LE:
      return op_le;
    case FunctionBuilder::EQ:
      return op_eq;
    case FunctionBuilder::NE:
      return op_ne;
    case FunctionBuilder::UGT:
      return op_ugt;
    case FunctionBuilder::UGE:
      return op_uge;
    case FunctionBuilder::ULT:
      return op_ult;
    case FunctionBuilder::ULE:
      return op_ule;
  }
}

class ANDispTable : public ANode {
 public:
  void AddPublic(std::string name, std::size_t index,
                 ForwardRef<ANode*>* target) {
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

void ObjectCodegen::AppendProperty(std::string name, std::uint16_t selectorNum,
                                   LiteralValue value) {
  value.visit(
      [&](int num) {
        props_->getList()->newNode<ANIntProp>(std::move(name), num);
      },
      [&](TextRef text) {
        props_->getList()->newNode<ANOfsProp>(std::move(name), text.ref_);
      });
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
                                 PtrRef* ptr_ref) {
  auto* entry = methDict_->getList()->newNode<ANComposite<ANode>>();
  entry->getList()->newNode<ANWord>(selectorNum);
  auto* method = entry->getList()->newNode<ANMethod>(std::move(name), nullptr);
  ptr_ref->ref_.RegisterCallback([method](ANode* target) {
    method->method = static_cast<ANCodeBlk*>(target);
  });
}

std::unique_ptr<ObjectCodegen> ObjectCodegen::Create(CodeGenerator* compiler,
                                                     bool isObj,
                                                     std::string name,
                                                     ForwardRef<ANode*>* ref) {
  // Allocate tables in the correct places in the heap/hunk.
  //
  // This does not actually allocate any space, but it does create
  // growable tables.
  compiler->objPropList->newNode<ANObject>(name);
  ANTable* props = compiler->objPropList->newNode<ANTable>("properties");

  ref->Resolve(props);

  compiler->objDictList->newNode<ANObject>(name);
  ANObjTable* propDict =
      compiler->objDictList->newNode<ANObjTable>("property dictionary");
  // The size of the method dictionary.
  ANCountWord* methDictSize =
      compiler->objDictList->newNode<ANCountWord>(nullptr);
  ANObjTable* methDict =
      compiler->objDictList->newNode<ANObjTable>("method dictionary");
  methDictSize->target = methDict->getList();
  return absl::WrapUnique(
      new ObjectCodegen(isObj, name, props, propDict, methDictSize, methDict));
}

ObjectCodegen::ObjectCodegen(bool isObj, std::string name, ANTable* props,
                             ANObjTable* propDict, ANode* methDictStart,
                             ANObjTable* methDict)
    : isObj_(isObj),
      name_(name),
      props_(props),
      propDict_(propDict),
      methDictStart_(methDictStart),
      methDict_(methDict) {}

void ObjectCodegen::AppendPropDict(std::uint16_t selectorNum) {
  if (!isObj_) {
    propDict_->getList()->newNode<ANWord>(selectorNum);
  }
}

// -----------------

LabelRef FunctionBuilder::CreateLabelRef() { return LabelRef(); }

void FunctionBuilder::AddLineAnnotation(std::size_t lineNum) {
  if (target_->SupportsDebugInstructions()) {
    code_node_->getList()->newNode<ANLineNum>(lineNum);
  }
}

void FunctionBuilder::AddPushOp() {
  code_node_->getList()->newNode<ANOpCode>(op_push);
}

void FunctionBuilder::AddPushImmediate(int value) {
  code_node_->getList()->newNode<ANOpUnsign>(op_pushi, value);
}

void FunctionBuilder::AddPushPrevOp() {
  code_node_->getList()->newNode<ANOpCode>(op_pprev);
}

void FunctionBuilder::AddTossOp() {
  code_node_->getList()->newNode<ANOpCode>(op_toss);
}

void FunctionBuilder::AddDupOp() {
  code_node_->getList()->newNode<ANOpCode>(op_dup);
}

void FunctionBuilder::AddRestOp(std::size_t value) {
  uint8_t op = op_rest;
  if (value < 256) {
    op |= OP_BYTE;
  }
  code_node_->getList()->newNode<ANOpUnsign>(op, value);
}

void FunctionBuilder::AddLoadImmediate(LiteralValue value) {
  value.visit(
      [this](int num) {
        code_node_->getList()->newNode<ANOpSign>(op_loadi, num);
      },
      [this](TextRef text) {
        code_node_->getList()->newNode<ANOpOfs>(text.ref_);
      });
}

void FunctionBuilder::AddLoadOffsetTo(PtrRef* ptr,
                                      std::optional<std::string> name) {
  auto* ofs = code_node_->getList()->newNode<ANObjID>(std::move(name));
  ptr->ref_.RegisterCallback([ofs](ANode* target) { ofs->target = target; });
}

void FunctionBuilder::AddLoadVarAddr(VarType var_type, std::size_t offset,
                                     bool addAccumIndex,
                                     std::optional<std::string> name) {
  uint32_t accType;
  switch (var_type) {
    case GLOBAL:
      accType = OP_GLOBAL;
      break;
    case LOCAL:
      accType = OP_LOCAL;
      break;
    case TEMP:
      accType = OP_TMP;
      break;
    case PARAM:
      accType = OP_PARM;
      break;
  }

  if (addAccumIndex) {
    accType |= OP_INDEX;
  }

  auto* node =
      code_node_->getList()->newNode<ANEffctAddr>(op_lea, offset, accType);
  if (name) {
    node->name = std::move(name).value();
  }
}

void FunctionBuilder::AddVarAccess(VarType var_type, ValueOp value_op,
                                   std::size_t offset, bool add_accum_index,
                                   std::optional<std::string> name) {
  uint8_t op = OP_LDST;
  if (value_op == STORE) {
    // Since the stored value is on the stack, we need to change the
    // instruction to read the stored value from the stack.
    //
    // This was missing from the original code. I have no idea how the
    // generated code worked without this.
    op |= OP_STACK;
  }

  // Check for indexing and compile the index if necessary.
  if (add_accum_index) {
    op |= OP_INDEX;  // set the indexing bit
  }

  // Set the bits indicating the type of variable to be accessed, then
  // put out the opcode to access it.
  switch (var_type) {
    case GLOBAL:
      op |= OP_GLOBAL;
      break;
    case LOCAL:
      op |= OP_LOCAL;
      break;
    case TEMP:
      op |= OP_TMP;
      break;
    case PARAM:
      op |= OP_PARM;
      break;
    default:
      throw std::runtime_error(
          "Internal error: bad variable type in MakeAccess()");
      break;
  }

  switch (value_op) {
    case LOAD:
      op |= OP_LOAD;
      break;
    case STORE:
      op |= OP_STORE;
      break;
    case INC:
      op |= OP_INC;
      break;
    case DEC:
      op |= OP_DEC;
      break;
  }

  if (offset < 256) op |= OP_BYTE;
  auto* an = code_node_->getList()->newNode<ANVarAccess>(op, offset);
  if (name) {
    an->name = std::move(name).value();
  }
}

void FunctionBuilder::AddPropAccess(ValueOp value_op, std::size_t offset,
                                    std::optional<std::string> name) {
  // Set the bits indicating the type of variable to be accessed, then
  // put out the opcode to access it.
  uint8_t op;
  switch (value_op) {
    case LOAD:
      op = op_pToa;
      break;
    case STORE:
      op = op_aTop;
      break;
    case INC:
      op = op_ipToa;
      break;
    case DEC:
      op = op_dpToa;
      break;
  }

  if (offset < 256) op |= OP_BYTE;
  auto* an = code_node_->getList()->newNode<ANVarAccess>(op, offset);
  if (name) {
    an->name = std::move(name).value();
  }
}

void FunctionBuilder::AddLoadClassOp(std::string name, std::size_t class_num) {
  auto* node = code_node_->getList()->newNode<ANOpUnsign>(op_class, class_num);
  node->name = std::move(name);
}
void FunctionBuilder::AddLoadSelfOp() {
  code_node_->getList()->newNode<ANOpCode>(op_selfID);
}

void FunctionBuilder::AddUnOp(UnOp op) {
  uint8_t opcode;
  switch (op) {
    case NEG:
      opcode = op_neg;
      break;
    case NOT:
      opcode = op_not;
      break;
    case BNOT:
      opcode = op_bnot;
      break;
  }
  code_node_->getList()->newNode<ANOpCode>(opcode);
}

void FunctionBuilder::AddBinOp(BinOp op) {
  code_node_->getList()->newNode<ANOpCode>(GetBinOpValue(op));
}

void FunctionBuilder::AddBranchOp(BranchOp op, LabelRef* target) {
  uint8_t opcode;
  switch (op) {
    case BNT:
      opcode = op_bnt;
      break;
    case BT:
      opcode = op_bt;
      break;
    case JMP:
      opcode = op_jmp;
      break;
  }
  auto* branch = code_node_->getList()->newNode<ANBranch>(opcode);
  target->ref_.RegisterCallback(
      [branch](ANLabel* target) { branch->target = target; });
}

void FunctionBuilder::AddLabel(LabelRef* label) {
  auto* an_label = code_node_->getList()->newNode<ANLabel>();
  label->ref_.Resolve(an_label);
}

void FunctionBuilder::AddProcCall(std::string name, std::size_t numArgs,
                                  PtrRef* target) {
  ANCall* call =
      code_node_->getList()->newNode<ANCall>(std::move(name), target_);
  call->numArgs = 2 * numArgs;
  target->ref_.RegisterCallback(
      [call](ANode* target) { call->target = target; });
}

void FunctionBuilder::AddExternCall(std::string name, std::size_t numArgs,
                                    std::size_t script_num, std::size_t entry) {
  ANOpExtern* ext_call = code_node_->getList()->newNode<ANOpExtern>(
      std::move(name), target_, script_num, entry);
  ext_call->numArgs = 2 * numArgs;
}

void FunctionBuilder::AddKernelCall(std::string name, std::size_t numArgs,
                                    std::size_t entry) {
  ANOpExtern* ext_call = code_node_->getList()->newNode<ANOpExtern>(
      std::move(name), target_, -1, entry);
  ext_call->numArgs = 2 * numArgs;
}

void FunctionBuilder::AddSend(std::size_t numArgs) {
  ANSend* send = code_node_->getList()->newNode<ANSend>(target_, op_send);
  send->numArgs = 2 * numArgs;
}

void FunctionBuilder::AddSelfSend(std::size_t numArgs) {
  ANSend* send = code_node_->getList()->newNode<ANSend>(target_, op_self);
  send->numArgs = 2 * numArgs;
}

void FunctionBuilder::AddSuperSend(std::string name, std::size_t numArgs,
                                   std::size_t species) {
  auto* send = code_node_->getList()->newNode<ANSuper>(target_, std::move(name),
                                                       species);
  send->numArgs = 2 * numArgs;
}

void FunctionBuilder::AddReturnOp() {
  code_node_->getList()->newNode<ANOpCode>(op_ret);
}

FunctionBuilder::FunctionBuilder(SciTargetStrategy const* target,
                                 ANCodeBlk* root_node)
    : target_(target), code_node_(root_node) {}

// -----------------

std::unique_ptr<CodeGenerator> CodeGenerator::Create(Options options) {
  SciTargetStrategy const* target_strategy;
  switch (options.target) {
    case SciTarget::SCI_1_1:
      target_strategy = SciTargetStrategy::GetSci11();
      break;
    case SciTarget::SCI_2:
      target_strategy = SciTargetStrategy::GetSci2();
      break;
    default:
      throw std::runtime_error("Invalid target architecture");
  }
  auto compiler = absl::WrapUnique(new CodeGenerator());
  compiler->sci_target = target_strategy;
  compiler->opt = options.opt;
  compiler->InitAsm();
  return compiler;
}

CodeGenerator::CodeGenerator() : active(false) {
  hunkList = std::make_unique<FixupList>();
  heapList = std::make_unique<FixupList>();
}

CodeGenerator::~CodeGenerator() = default;

void CodeGenerator::InitAsm() {
  if (active) {
    throw std::runtime_error("Compiler already active");
  }
  // Initialize the assembly list: dispose of any old list, then add
  // nodes for the number of local variables.

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
  active = true;
}

void CodeGenerator::Assemble(std::string_view source_file_name,
                             uint16_t scriptNum, ListingFile* listFile) {
  if (!active) {
    throw std::runtime_error("Compiler not active");
  }

  // Set the offsets in the object list.
  heapList->setOffset(0);

  // Optimize the code, setting all the offsets.
  OptimizeHunk(opt, hunkList->getRoot());

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
  absl::FPrintF(infoFile, "%s\n", source_file_name);
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

  hunkList = nullptr;
  heapList = nullptr;

  active = false;
}

PtrRef CodeGenerator::CreatePtrRef() { return PtrRef(); }

void CodeGenerator::AddPublic(std::string name, std::size_t index,
                              PtrRef* target) {
  dispTable->AddPublic(std::move(name), index, &target->ref_);
}

bool CodeGenerator::IsInHeap(ANode const* node) {
  return heapList->contains(node);
}

TextRef CodeGenerator::AddTextNode(std::string_view text) {
  auto it = textNodes.find(text);
  if (it != textNodes.end()) return TextRef(it->second);
  auto* textNode = textList->newNode<ANText>(std::string(text));

  textNodes.emplace(std::string(text), textNode);
  return TextRef(textNode);
}

std::size_t CodeGenerator::NumVars() const { return localVars.size(); }

bool CodeGenerator::SetVar(std::size_t varNum, LiteralValue value) {
  if (localVars.size() <= varNum) {
    localVars.resize(varNum + 1);
  }

  auto& vp = localVars[varNum];
  if (vp.value) {
    return false;
  }

  vp.value = value;
  return true;
}

std::unique_ptr<ObjectCodegen> CodeGenerator::CreateObject(std::string name,
                                                           PtrRef* ref) {
  return ObjectCodegen::Create(this, true, name, &ref->ref_);
}

std::unique_ptr<ObjectCodegen> CodeGenerator::CreateClass(std::string name,
                                                          PtrRef* ref) {
  return ObjectCodegen::Create(this, false, name, &ref->ref_);
}

std::unique_ptr<FunctionBuilder> CodeGenerator::CreateFunction(
    FuncName name, std::optional<std::size_t> lineNum, std::size_t numTemps,
    PtrRef* ptr_ref) {
  ANCodeBlk* code = std::move(name).visit(
      [&](ProcedureName name) -> ANCodeBlk* {
        return codeList->newNode<ANProcCode>(std::move(name.procName));
      },
      [&](MethodName name) -> ANCodeBlk* {
        return codeList->newNode<ANMethCode>(std::move(name.methName),
                                             std::move(name.objName));
      });

  ptr_ref->ref_.Resolve(code);

  if (sci_target->SupportsDebugInstructions() && lineNum) {
    // If supported by the configuration, add line number information.
    // procedures and methods get special treatment:  the line number
    // and file name are set here
    code->getList()->newNode<ANLineNum>(*lineNum);
  }

  // If there are to be any temporary variables, add a link node to
  // create them.
  if (numTemps > 0) {
    code->getList()->newNode<ANOpUnsign>(op_link, numTemps);
  }

  return absl::WrapUnique(new FunctionBuilder(sci_target, code));
}

}  // namespace codegen
