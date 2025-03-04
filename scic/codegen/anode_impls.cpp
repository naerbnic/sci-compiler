//	anode.cpp
// 	assemble an object code list

#include "scic/codegen/anode_impls.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "absl/strings/str_format.h"
#include "scic/codegen/alist.hpp"
#include "scic/codegen/anode.hpp"
#include "scic/codegen/common.hpp"
#include "scic/codegen/optimize.hpp"
#include "scic/codegen/output.hpp"
#include "scic/codegen/target.hpp"
#include "scic/listing.hpp"
#include "scic/opcodes.hpp"

namespace codegen {

#define OPTIMIZE_TRANSFERS

static bool canOptimizeTransfer(size_t a, size_t b) {
  size_t larger = std::max(a, b);
  size_t smaller = std::min(a, b);

  return (larger - smaller) < 128;
}

// In SCI 1.1 and earlier, Calls and Sends wrote the number of args as
// one byte.  In SCI 2, it's two bytes.  These functions abstract those
// architecture differences out.

///////////////////////////////////////////////////
// Class ANDispatch
///////////////////////////////////////////////////

void ANDispatch::list(ListingFile* listFile) const {
  std::size_t curOfs = *offset;

  if (target && name)
    listFile->ListAsCode(curOfs, "dispatch\t$%-4x\t(%s)", *target->offset,
                         *name);
  else if (name)
    listFile->ListAsCode(curOfs, "dispatch\t----\t(%s)", *name);
  else
    listFile->ListAsCode(curOfs, "dispatch\t----");
}

size_t ANDispatch::size() const { return 2; }

void ANDispatch::collectFixups(FixupContext* fixup_ctxt) const {
  // If the destination of this dispatch entry is in the heap (i.e.
  // an object rather than code), it must be fixed up.
  if (fixup_ctxt->HeapHasNode(target)) fixup_ctxt->AddRelFixup(this, 0);
}

void ANDispatch::emit(OutputWriter* out) const {
  out->WriteWord((uint32_t)(target && name ? *target->offset : 0));
}

///////////////////////////////////////////////////
// Class ANWord
///////////////////////////////////////////////////

ANWord::ANWord(int v) : value(v) {}

size_t ANWord::size() const { return 2; }

void ANWord::list(ListingFile* listFile) const {
  listFile->ListWord(*offset, value);
}

void ANWord::emit(OutputWriter* out) const { out->WriteWord(value); }

///////////////////////////////////////////////////
// Class ANTable
///////////////////////////////////////////////////

ANTable::ANTable(std::string nameStr) : name(nameStr) {}

void ANTable::list(ListingFile* listFile) const {
  listFile->Listing("\t\t(%s)", name);
  ANComposite::list(listFile);
}

///////////////////////////////////////////////////
// Class ANObjTable
///////////////////////////////////////////////////

ANObjTable::ANObjTable(std::string nameStr) : ANTable(nameStr) {}

///////////////////////////////////////////////////
// Class ANText
///////////////////////////////////////////////////

ANText::ANText(std::string text) : text(std::move(text)) {}

size_t ANText::setOffset(size_t ofs) { return ANode::setOffset(ofs); }

size_t ANText::size() const { return text.size() + 1; }

void ANText::list(ListingFile* listFile) const {
  listFile->ListText(*offset, text);
}

void ANText::emit(OutputWriter* out) const {
  out->WriteNullTerminatedString(text);
}

///////////////////////////////////////////////////
// Class ANObject
///////////////////////////////////////////////////

ANObject::ANObject(std::string name) : name(std::move(name)) {}

void ANObject::list(ListingFile* listFile) const {
  listFile->Listing("\nObject: %-20s", name);
}

///////////////////////////////////////////////////
// Class ANCodeBlk
///////////////////////////////////////////////////

ANCodeBlk::ANCodeBlk(std::string name) : name(std::move(name)) {
  ANLabel::reset();
}

bool ANCodeBlk::optimize() { return OptimizeProc(getList()); }

///////////////////////////////////////////////////
// Class ANProcCode
///////////////////////////////////////////////////

void ANProcCode::list(ListingFile* listFile) const {
  listFile->Listing("\n\nProcedure: (%s)\n", name);
  ANCodeBlk::list(listFile);
}

///////////////////////////////////////////////////
// Class ANMethCode
///////////////////////////////////////////////////

ANMethCode::ANMethCode(std::string name, std::string obj_name)
    : ANCodeBlk(std::move(name)), obj_name(std::move(obj_name)) {}

void ANMethCode::list(ListingFile* listFile) const {
  listFile->Listing("\n\nMethod: (%s %s)\n", obj_name, name);
  ANCodeBlk::list(listFile);
}

///////////////////////////////////////////////////
// Class ANProp
///////////////////////////////////////////////////

ANProp::ANProp(std::string name) : name(std::move(name)) {}

size_t ANProp::size() const { return 2; }

void ANProp::list(ListingFile* listFile) const {
  listFile->ListAsCode(*offset, "%-6s$%-4x\t(%s)", desc(), (SCIUWord)value(),
                       name);
}

void ANProp::emit(OutputWriter* out) const { out->WriteWord(value()); }

std::string_view ANIntProp::desc() const { return "prop"; }

uint32_t ANIntProp::value() const { return val; }

void ANOfsProp::collectFixups(FixupContext* fixup_ctxt) const {
  if (fixup_ctxt->HeapHasNode(target)) fixup_ctxt->AddRelFixup(this, 0);
}
std::string_view ANOfsProp::desc() const { return "ofs"; }

uint32_t ANOfsProp::value() const { return *target->offset; }

ANMethod::ANMethod(std::string name, ANCodeBlk* mp)
    : ANProp(std::move(name)), method(mp) {}

std::string_view ANMethod::desc() const { return "local"; }

uint32_t ANMethod::value() const { return *method->offset; }

///////////////////////////////////////////////////
// Class ANLabel
///////////////////////////////////////////////////

uint32_t ANLabel::nextLabel = 0;

ANLabel::ANLabel() : ANOpCode(OP_LABEL), number(nextLabel++) {}

size_t ANLabel::size() const { return 0; }

void ANLabel::list(ListingFile* listFile) const {
  listFile->Listing(".%d", number);
}

void ANLabel::emit(OutputWriter*) const {}

///////////////////////////////////////////////////
// Class ANOpUnsign
///////////////////////////////////////////////////

ANOpUnsign::ANOpUnsign(uint32_t o, uint32_t v) {
  value = v;
#if defined(OPTIMIZE_TRANSFERS)
  op = o | (value < 256 ? OP_BYTE : 0);
#else
  if (o == op_link || o == op_class)
    op = o | (value < 256 ? OP_BYTE : 0);
  else
    op = o | (value < 128 ? OP_BYTE : 0);
#endif
  name = std::nullopt;
}

size_t ANOpUnsign::size() const { return op & OP_BYTE ? 2 : 3; }

void ANOpUnsign::list(ListingFile* listFile) const {
  listFile->ListOp(*offset, op);
  if (!name)
    listFile->ListArg("$%-4x", (SCIUWord)value);
  else
    listFile->ListArg("$%-4x\t(%s)", (SCIUWord)value, *name);
}

void ANOpUnsign::emit(OutputWriter* out) const {
  out->WriteOp(op);
  if (op & OP_BYTE)
    out->WriteByte(value);
  else
    out->WriteWord(value);
}

///////////////////////////////////////////////////
// Class ANOpSign
///////////////////////////////////////////////////

ANOpSign::ANOpSign(uint32_t o, int v) {
  value = v;
  op = o | ((uint32_t)abs(value) < 128 ? OP_BYTE : 0);
}

size_t ANOpSign::size() const { return op & OP_BYTE ? 2 : 3; }

void ANOpSign::list(ListingFile* listFile) const {
  listFile->ListOp(*offset, op);
  if (!name)
    listFile->ListArg("$%-4x", (SCIUWord)value);
  else
    listFile->ListArg("$%-4x\t(%s)", (SCIUWord)value, *name);
}

void ANOpSign::emit(OutputWriter* out) const {
  out->WriteOp(op);
  if (op & OP_BYTE)
    out->WriteByte(value);
  else
    out->WriteWord(value);
}

///////////////////////////////////////////////////
// Class ANOpExtern
///////////////////////////////////////////////////

uint8_t GetExternOp(int32_t module, uint32_t entry) {
  if (module == -1) {
    return op_callk | (entry < 256 ? OP_BYTE : 0);
  }

  if (module == 0) {
    return op_callb | (entry < 256 ? OP_BYTE : 0);
  }

  if (module < 0) {
    throw std::runtime_error("Invalid module number");
  }

  return op_calle | (module < 256 && entry < 256 ? OP_BYTE : 0);
}

ANOpExtern::ANOpExtern(std::string name, SciTargetStrategy const* sci_target,
                       int32_t m, uint32_t e)
    : sci_target(sci_target), module(m), entry(e), name(std::move(name)) {
  op = GetExternOp(module, entry);
}

size_t ANOpExtern::size() const {
  int arg_size = sci_target->NumArgsSize();

  switch (op & ~OP_BYTE) {
    case op_callk:
    case op_callb:
      return (op & OP_BYTE ? 2 : 3) + arg_size;
    case op_calle:
      return (op & OP_BYTE ? 3 : 5) + arg_size;
    default:
      return 0;
  }
}

void ANOpExtern::list(ListingFile* listFile) const {
  listFile->ListOp(*offset, op);
  switch (op & ~OP_BYTE) {
    case op_callk:
    case op_callb:
      listFile->ListArg("$%-4x\t(%s)", (SCIUWord)entry, name);
      break;
    case op_calle:
      listFile->ListArg("$%x/%x\t(%s)", (SCIUWord)module, (SCIUWord)entry,
                        name);
  }
  sci_target->ListNumArgs(listFile, *offset + 1, numArgs);
}

void ANOpExtern::emit(OutputWriter* out) const {
  out->WriteOp(op);
  if ((op & ~OP_BYTE) == op_calle) {
    if (op & OP_BYTE)
      out->WriteByte(module);
    else
      out->WriteWord(module);
  }

  if (op & OP_BYTE)
    out->WriteByte(entry);
  else
    out->WriteWord(entry);
  sci_target->WriteNumArgs(out, numArgs);
}

///////////////////////////////////////////////////
// Class ANCall
///////////////////////////////////////////////////

ANCall::ANCall(std::string name, SciTargetStrategy const* sci_target)
    : ANOpCode(op_call),
      sci_target(sci_target),
      name(std::move(name)),
      target(nullptr) {}

size_t ANCall::size() const {
  int arg_size = sci_target->NumArgsSize();

  return (op & OP_BYTE ? 2 : 3) + arg_size;
}

bool ANCall::tryShrink() {
  auto initial_size = size();

  if (!target || !target->offset.has_value()) {
    return false;
  }

#if defined(OPTIMIZE_TRANSFERS)
  if (canOptimizeTransfer(*target->offset, *offset + 5)) {
    op |= OP_BYTE;
  } else
#endif
  {
    op &= ~OP_BYTE;
  }

  return size() < initial_size;
}

void ANCall::list(ListingFile* listFile) const {
  listFile->ListOp(*offset, op_call);
  listFile->ListArg("$%-4x\t(%s)",
                    SCIUWord(*target->offset - (*offset + size())), name);
  sci_target->ListNumArgs(listFile, *offset + 1, numArgs);
}

void ANCall::emit(OutputWriter* out) const {
  if (!target || !target->offset.has_value()) {
    throw std::runtime_error(absl::StrFormat("Undefined procedure: %s", name));
    return;
  }

  int n = *target->offset - (*offset + size());
  out->WriteOp(op);
  if (op & OP_BYTE)
    out->WriteByte(n);
  else
    out->WriteWord(n);
  sci_target->WriteNumArgs(out, numArgs);
}

///////////////////////////////////////////////////
// Class ANBranch
///////////////////////////////////////////////////

ANBranch::ANBranch(uint32_t o) : target(nullptr) { op = o; }

size_t ANBranch::size() const { return op & OP_BYTE ? 2 : 3; }

bool ANBranch::tryShrink() {
  auto initial_size = size();
  if (!target || !target->offset.has_value()) {
    return false;
  }
#if defined(OPTIMIZE_TRANSFERS)

  if (canOptimizeTransfer(*target->offset, *offset + 4)) {
    op |= OP_BYTE;
  } else
#endif
  {
    op &= ~OP_BYTE;
  }
  return size() < initial_size;
}

void ANBranch::list(ListingFile* listFile) const {
  listFile->ListOp(*offset, op);
  listFile->ListArg("$%-4x\t(.%d)",
                    SCIUWord(*target->offset - (*offset + size())),
                    ((ANLabel*)target)->number);
}

void ANBranch::emit(OutputWriter* out) const {
  int n = *target->offset - (*offset + size());

  out->WriteOp(op);
  if (op & OP_BYTE)
    out->WriteByte(n);
  else
    out->WriteWord(n);
}

///////////////////////////////////////////////////
// Class ANVarAccess
///////////////////////////////////////////////////

ANVarAccess::ANVarAccess(uint32_t o, uint32_t a) : addr(a) {
  op = addr < 256 ? o | OP_BYTE : o;
}

size_t ANVarAccess::size() const { return op & OP_BYTE ? 2 : 3; }

void ANVarAccess::list(ListingFile* listFile) const {
  listFile->ListOp(*offset, op);
  if (name)
    listFile->ListArg("$%-4x\t(%s)", addr, *name);
  else
    listFile->ListArg("$%-4x", addr);
}

void ANVarAccess::emit(OutputWriter* out) const {
  out->WriteOp(op);
  if (op & OP_BYTE)
    out->WriteByte(addr);
  else
    out->WriteWord(addr);
}

///////////////////////////////////////////////////
// Class ANOpOfs
///////////////////////////////////////////////////

ANOpOfs::ANOpOfs(ANText* text) : ANOpCode(op_lofsa), text(text) {}

size_t ANOpOfs::size() const { return WORDSIZE; }

void ANOpOfs::list(ListingFile* listFile) const {
  listFile->ListOp(*offset, op);
  listFile->ListArg("$%-4x", *text->offset);
}

void ANOpOfs::collectFixups(FixupContext* fixup_ctxt) const {
  fixup_ctxt->AddRelFixup(this, 1);
}

void ANOpOfs::emit(OutputWriter* out) const {
  out->WriteOp(op);
  out->WriteWord(*text->offset);
}

///////////////////////////////////////////////////
// Class ANObjID
///////////////////////////////////////////////////

ANObjID::ANObjID(std::optional<std::string> name)
    : ANOpCode(op_lofsa), name(std::move(name)) {}

size_t ANObjID::size() const { return WORDSIZE; }

void ANObjID::list(ListingFile* listFile) const {
  listFile->ListOp(*offset, op);
  if (name) {
    listFile->ListArg("$%-4x\t(%s)", *target->offset, *name);
  } else {
    listFile->ListArg("$%-4x", *target->offset);
  }
}

void ANObjID::collectFixups(FixupContext* fixup_ctxt) const {
  fixup_ctxt->AddRelFixup(this, 1);
}
void ANObjID::emit(OutputWriter* out) const {
  out->WriteOp(op);
  out->WriteWord(*target->offset);
}

///////////////////////////////////////////////////
// Class ANEffctAddr
///////////////////////////////////////////////////

ANEffctAddr::ANEffctAddr(uint32_t o, uint32_t a, uint32_t t)
    : ANVarAccess(o, a), eaType(t) {}

size_t ANEffctAddr::size() const { return op & OP_BYTE ? 3 : 5; }

void ANEffctAddr::list(ListingFile* listFile) const {
  listFile->ListOp(*offset, op);
  listFile->ListArg("$%-4x\t(%s)", addr, *name);
}

void ANEffctAddr::emit(OutputWriter* out) const {
  out->WriteOp(op);
  if (op & OP_BYTE) {
    out->WriteByte(eaType);
    out->WriteByte(addr);

  } else {
    out->WriteWord(eaType);
    out->WriteWord(addr);
  }
}

///////////////////////////////////////////////////
// Class ANSend
///////////////////////////////////////////////////

ANSend::ANSend(SciTargetStrategy const* sci_target, uint32_t o)
    : ANOpCode(o), sci_target(sci_target) {}

size_t ANSend::size() const { return 1 + sci_target->NumArgsSize(); }

void ANSend::list(ListingFile* listFile) const {
  listFile->ListOp(*offset, op);
  sci_target->ListNumArgs(listFile, *offset + 1, numArgs);
}

void ANSend::emit(OutputWriter* out) const {
  out->WriteOp(op);
  sci_target->WriteNumArgs(out, numArgs);
}

///////////////////////////////////////////////////
// Class ANSuper
///////////////////////////////////////////////////

ANSuper::ANSuper(SciTargetStrategy const* sci_target, std::string name,
                 uint32_t c)
    : ANSend(sci_target, op_super), classNum(c), name(std::move(name)) {
  if (classNum < 256) op |= OP_BYTE;
}

size_t ANSuper::size() const {
  return (op & OP_BYTE ? 2 : 3) + sci_target->NumArgsSize();
}

void ANSuper::list(ListingFile* listFile) const {
  listFile->ListOp(*offset, op);
  listFile->ListArg("$%-4x\t(%s)", classNum, name);
  sci_target->ListNumArgs(listFile, *offset + 1, numArgs);
}

void ANSuper::emit(OutputWriter* out) const {
  out->WriteOp(op);
  if (op & OP_BYTE)
    out->WriteByte(classNum);
  else
    out->WriteWord(classNum);
  sci_target->WriteNumArgs(out, numArgs);
}

//////////////////////////////////////////////////////////////////////////////

ANFileName::ANFileName(std::string name) : ANOpCode(op_fileName), name(name) {}

void ANFileName::list(ListingFile* listFile) const {
  listFile->ListOffset(*offset);
  listFile->Listing("file");
}

void ANFileName::emit(OutputWriter* out) const {
  out->WriteOp(op);
  out->WriteNullTerminatedString(name);
}

size_t ANFileName::size() const { return 1 + name.length() + 1; }

////////////////////////////////////////////////////////////////////////////

ANLineNum::ANLineNum(int num) : ANOpCode(op_lineNum), num(num) {}

void ANLineNum::list(ListingFile* listFile) const {
  listFile->ListSourceLine(num);
}

void ANLineNum::emit(OutputWriter* out) const {
  out->WriteOp(op);
  out->WriteWord(num);
}

size_t ANLineNum::size() const { return 1 + sizeof(SCIWord); }

}  // namespace codegen
