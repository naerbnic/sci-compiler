//	anode.cpp
// 	assemble an object code list

#include "scic/anode.hpp"

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
#include "scic/alist.hpp"
#include "scic/common.hpp"
#include "scic/config.hpp"
#include "scic/listing.hpp"
#include "scic/opcodes.hpp"
#include "scic/optimize.hpp"
#include "scic/output.hpp"
#include "scic/symtypes.hpp"
#include "scic/text.hpp"
#include "scic/varlist.hpp"

ANCodeBlk* gCodeStart;
ANTable* gTextTable;

#define OPTIMIZE_TRANSFERS

static bool canOptimizeTransfer(size_t a, size_t b) {
  size_t larger = std::max(a, b);
  size_t smaller = std::min(a, b);

  return (larger - smaller) < 128;
}

// In SCI 1.1 and earlier, Calls and Sends wrote the number of args as
// one byte.  In SCI 2, it's two bytes.  These functions abstract those
// architecture differences out.

static int NumArgsSize() {
  switch (gConfig->targetArch) {
    case SciTargetArch::SCI_1_1:
      return 1;

    case SciTargetArch::SCI_2:
      return 2;

    default:
      throw std::runtime_error("Invalid target architecture");
  }
}

static void ListNumArgs(ListingFile* listFile, std::size_t offset, int n) {
  switch (gConfig->targetArch) {
    case SciTargetArch::SCI_1_1:
      listFile->ListByte(offset, n);
      break;

    case SciTargetArch::SCI_2:
      listFile->ListWord(offset, n);
      break;

    default:
      throw std::runtime_error("Invalid target architecture");
  }
}

static void WriteNumArgs(OutputFile* out, int n) {
  switch (gConfig->targetArch) {
    case SciTargetArch::SCI_1_1:
      out->WriteByte(n);
      break;

    case SciTargetArch::SCI_2:
      out->WriteWord(n);
      break;

    default:
      throw std::runtime_error("Invalid target architecture");
  }
}

///////////////////////////////////////////////////
// Class ANDispatch
///////////////////////////////////////////////////

void ANDispatch::list(ListingFile* listFile) {
  std::size_t curOfs = *offset;

  if (target && name)
    listFile->ListAsCode(curOfs, "dispatch\t$%-4x\t(%s)", *target->offset,
                         *name);
  else if (name)
    listFile->ListAsCode(curOfs, "dispatch\t----\t(%s)", *name);
  else
    listFile->ListAsCode(curOfs, "dispatch\t----");
}

size_t ANDispatch::size() { return 2; }

void ANDispatch::emit(FixupContext* fixup_ctxt, OutputFile* out) {
  // If the destination of this dispatch entry is in the heap (i.e.
  // an object rather than code), it must be fixed up.

  if (fixup_ctxt->HeapHasNode(target)) fixup_ctxt->AddRelFixup(this, 0);

  out->WriteWord((uint32_t)(target && name ? *target->offset : 0));
}

///////////////////////////////////////////////////
// Class ANWord
///////////////////////////////////////////////////

ANWord::ANWord(int v) : value(v) {}

size_t ANWord::size() { return 2; }

void ANWord::list(ListingFile* listFile) { listFile->ListWord(*offset, value); }

void ANWord::emit(FixupContext* fixup_ctxt, OutputFile* out) {
  out->WriteWord(value);
}

///////////////////////////////////////////////////
// Class ANTable
///////////////////////////////////////////////////

ANTable::ANTable(std::string nameStr) : name(nameStr) {}

size_t ANTable::size() { return entries.size(); }

size_t ANTable::setOffset(size_t ofs) {
  offset = ofs;
  return entries.setOffset(ofs);
}

void ANTable::list(ListingFile* listFile) {
  listFile->Listing("\t\t(%s)", name);
  for (auto& entry : entries) entry.list(listFile);
}

void ANTable::emit(FixupContext* fixup_ctxt, OutputFile* out) {
  entries.emit(fixup_ctxt, out);
}

///////////////////////////////////////////////////
// Class ANObjTable
///////////////////////////////////////////////////

ANObjTable::ANObjTable(std::string nameStr) : ANTable(nameStr) {}

///////////////////////////////////////////////////
// Class ANText
///////////////////////////////////////////////////

ANText::ANText(Text* tp) : text(tp) {}

size_t ANText::setOffset(size_t ofs) { return ANode::setOffset(ofs); }

size_t ANText::size() { return text->str.size() + 1; }

void ANText::list(ListingFile* listFile) {
  listFile->ListText(*offset, text->str);
}

void ANText::emit(FixupContext* fixup_ctxt, OutputFile* out) {
  out->WriteNullTerminatedString(text->str);
}

///////////////////////////////////////////////////
// Class ANObject
///////////////////////////////////////////////////

ANObject::ANObject(std::string name) : name(std::move(name)) {}

void ANObject::list(ListingFile* listFile) {
  listFile->Listing("\nObject: %-20s", name);
}

///////////////////////////////////////////////////
// Class ANCodeBlk
///////////////////////////////////////////////////

ANCodeBlk::ANCodeBlk(std::string name) : name(std::move(name)) {
  ANLabel::reset();
}

size_t ANCodeBlk::size() { return code.size(); }

void ANCodeBlk::emit(FixupContext* fixup_ctxt, OutputFile* out) {
  code.emit(fixup_ctxt, out);
}

size_t ANCodeBlk::setOffset(size_t ofs) {
  offset = ofs;
  return code.setOffset(ofs);
}

void ANCodeBlk::list(ListingFile* listFile) { code.list(listFile); }

bool ANCodeBlk::optimize() { return OptimizeProc(&code); }

///////////////////////////////////////////////////
// Class ANProcCode
///////////////////////////////////////////////////

void ANProcCode::list(ListingFile* listFile) {
  listFile->Listing("\n\nProcedure: (%s)\n", name);
  ANCodeBlk::list(listFile);
}

///////////////////////////////////////////////////
// Class ANMethCode
///////////////////////////////////////////////////

ANMethCode::ANMethCode(std::string name, std::string obj_name)
    : ANCodeBlk(std::move(name)), obj_name(std::move(obj_name)) {}

void ANMethCode::list(ListingFile* listFile) {
  listFile->Listing("\n\nMethod: (%s %s)\n", obj_name, name);
  ANCodeBlk::list(listFile);
}

///////////////////////////////////////////////////
// Class ANProp
///////////////////////////////////////////////////

ANProp::ANProp(std::string name, int v) : name(std::move(name)), val(v) {}

size_t ANProp::size() { return 2; }

void ANProp::list(ListingFile* listFile) {
  listFile->ListAsCode(*offset, "%-6s$%-4x\t(%s)", desc(), (SCIUWord)value(),
                       name);
}

void ANProp::emit(FixupContext* fixup_ctxt, OutputFile* out) {
  out->WriteWord(value());
}

std::string_view ANIntProp::desc() { return "prop"; }

uint32_t ANIntProp::value() { return val; }

ANTextProp::ANTextProp(std::string name, int v) : ANProp(std::move(name), v) {}

void ANTextProp::emit(FixupContext* fixup_ctxt, OutputFile* out) {
  fixup_ctxt->AddRelFixup(this, 0);
  ANProp::emit(fixup_ctxt, out);
}

std::string_view ANTextProp::desc() { return "text"; }

uint32_t ANTextProp::value() { return val + *gTextTable->offset; }

std::string_view ANOfsProp::desc() { return "ofs"; }

uint32_t ANOfsProp::value() { return *target->offset; }

ANMethod::ANMethod(std::string name, ANMethCode* mp)
    : ANProp(std::move(name), 0), method(mp) {}

std::string_view ANMethod::desc() { return "local"; }

uint32_t ANMethod::value() { return *method->offset; }

///////////////////////////////////////////////////
// Class ANOpCode
///////////////////////////////////////////////////

ANOpCode::ANOpCode(uint32_t o) : op(o) {}

size_t ANOpCode::size() { return 1; }

void ANOpCode::list(ListingFile* listFile) { listFile->ListOp(*offset, op); }

void ANOpCode::emit(FixupContext* fixup_ctxt, OutputFile* out) {
  out->WriteOp(op);
}

///////////////////////////////////////////////////
// Class ANLabel
///////////////////////////////////////////////////

uint32_t ANLabel::nextLabel = 0;

ANLabel::ANLabel() : ANOpCode(OP_LABEL), number(nextLabel++) {}

size_t ANLabel::size() { return 0; }

void ANLabel::list(ListingFile* listFile) { listFile->Listing(".%d", number); }

void ANLabel::emit(FixupContext*, OutputFile*) {}

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

size_t ANOpUnsign::size() { return op & OP_BYTE ? 2 : 3; }

void ANOpUnsign::list(ListingFile* listFile) {
  listFile->ListOp(*offset, op);
  if (!name)
    listFile->ListArg("$%-4x", (SCIUWord)value);
  else
    listFile->ListArg("$%-4x\t(%s)", (SCIUWord)value, *name);
}

void ANOpUnsign::emit(FixupContext* fixup_ctxt, OutputFile* out) {
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

size_t ANOpSign::size() { return op & OP_BYTE ? 2 : 3; }

void ANOpSign::list(ListingFile* listFile) {
  listFile->ListOp(*offset, op);
  if (!name)
    listFile->ListArg("$%-4x", (SCIUWord)value);
  else
    listFile->ListArg("$%-4x\t(%s)", (SCIUWord)value, *name);
}

void ANOpSign::emit(FixupContext* fixup_ctxt, OutputFile* out) {
  out->WriteOp(op);
  if (op & OP_BYTE)
    out->WriteByte(value);
  else
    out->WriteWord(value);
}

///////////////////////////////////////////////////
// Class ANOpExtern
///////////////////////////////////////////////////

ANOpExtern::ANOpExtern(std::string name, int32_t m, uint32_t e)
    : module(m), entry(e), name(std::move(name)) {
  switch (module) {
    case KERNEL:
      op = op_callk | (entry < 256 ? OP_BYTE : 0);
      break;
    case 0:
      op = op_callb | (entry < 256 ? OP_BYTE : 0);
      break;
    default:
      op = op_calle | (module < 256 && entry < 256 ? OP_BYTE : 0);
      break;
  }
}

size_t ANOpExtern::size() {
  int arg_size = NumArgsSize();

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

void ANOpExtern::list(ListingFile* listFile) {
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
  ListNumArgs(listFile, *offset + 1, numArgs);
}

void ANOpExtern::emit(FixupContext* fixup_ctxt, OutputFile* out) {
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
  WriteNumArgs(out, numArgs);
}

///////////////////////////////////////////////////
// Class ANCall
///////////////////////////////////////////////////

ANCall::ANCall(std::string name)
    : ANOpCode(op_call), name(std::move(name)), target(nullptr) {}

size_t ANCall::size() {
  int arg_size = NumArgsSize();

  if (!gShrink)
    return (op & OP_BYTE ? 2 : 3) + arg_size;
  else if (!target || !target->offset.has_value())
    return 3 + arg_size;
#if defined(OPTIMIZE_TRANSFERS)
  else if (canOptimizeTransfer(*target->offset, *offset + 5)) {
    op |= OP_BYTE;
    return 2 + arg_size;
  }
#endif
  else {
    op &= ~OP_BYTE;
    return 3 + arg_size;
  }
}

void ANCall::list(ListingFile* listFile) {
  listFile->ListOp(*offset, op_call);
  listFile->ListArg("$%-4x\t(%s)",
                    SCIUWord(*target->offset - (*offset + size())), name);
  ListNumArgs(listFile, *offset + 1, numArgs);
}

void ANCall::emit(FixupContext* fixup_ctxt, OutputFile* out) {
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
  WriteNumArgs(out, numArgs);
}

///////////////////////////////////////////////////
// Class ANBranch
///////////////////////////////////////////////////

ANBranch::ANBranch(uint32_t o) : target(nullptr) { op = o; }

size_t ANBranch::size() {
  if (!gShrink)
    return op & OP_BYTE ? 2 : 3;
  else if (!target || !target->offset.has_value())
    return 3;
#if defined(OPTIMIZE_TRANSFERS)
  else if (canOptimizeTransfer(*target->offset, *offset + 4)) {
    op |= OP_BYTE;
    return 2;
  }
#endif
  else {
    op &= ~OP_BYTE;
    return 3;
  }
}

void ANBranch::list(ListingFile* listFile) {
  listFile->ListOp(*offset, op);
  listFile->ListArg("$%-4x\t(.%d)",
                    SCIUWord(*target->offset - (*offset + size())),
                    ((ANLabel*)target)->number);
}

void ANBranch::emit(FixupContext* fixup_ctxt, OutputFile* out) {
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

size_t ANVarAccess::size() { return op & OP_BYTE ? 2 : 3; }

void ANVarAccess::list(ListingFile* listFile) {
  listFile->ListOp(*offset, op);
  if (name)
    listFile->ListArg("$%-4x\t(%s)", addr, *name);
  else
    listFile->ListArg("$%-4x", addr);
}

void ANVarAccess::emit(FixupContext* fixup_ctxt, OutputFile* out) {
  out->WriteOp(op);
  if (op & OP_BYTE)
    out->WriteByte(addr);
  else
    out->WriteWord(addr);
}

///////////////////////////////////////////////////
// Class ANOpOfs
///////////////////////////////////////////////////

ANOpOfs::ANOpOfs(uint32_t o) : ANOpCode(op_lofsa), ofs(o) {}

size_t ANOpOfs::size() { return WORDSIZE; }

void ANOpOfs::list(ListingFile* listFile) {
  listFile->ListOp(*offset, op);
  listFile->ListArg("$%-4x", *gTextTable->offset + ofs);
}

void ANOpOfs::emit(FixupContext* fixup_ctxt, OutputFile* out) {
  out->WriteOp(op);
  fixup_ctxt->AddRelFixup(this, 1);
  out->WriteWord(*gTextTable->offset + ofs);
}

///////////////////////////////////////////////////
// Class ANObjID
///////////////////////////////////////////////////

ANObjID::ANObjID(int lineNum, std::string name)
    : ANOpCode(op_lofsa), lineNum(lineNum), name(std::move(name)) {}

size_t ANObjID::size() { return WORDSIZE; }

void ANObjID::list(ListingFile* listFile) {
  listFile->ListOp(*offset, op);
  listFile->ListArg("$%-4x\t(%s)", *target->offset, name);
}

void ANObjID::emit(FixupContext* fixup_ctxt, OutputFile* out) {
  out->WriteOp(op);
  fixup_ctxt->AddRelFixup(this, 1);
  out->WriteWord(*target->offset);
}

///////////////////////////////////////////////////
// Class ANEffctAddr
///////////////////////////////////////////////////

ANEffctAddr::ANEffctAddr(uint32_t o, uint32_t a, uint32_t t)
    : ANVarAccess(o, a), eaType(t) {}

size_t ANEffctAddr::size() { return op & OP_BYTE ? 3 : 5; }

void ANEffctAddr::list(ListingFile* listFile) {
  listFile->ListOp(*offset, op);
  listFile->ListArg("$%-4x\t(%s)", addr, *name);
}

void ANEffctAddr::emit(FixupContext* fixup_ctxt, OutputFile* out) {
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

ANSend::ANSend(uint32_t o) : ANOpCode(o) {}

size_t ANSend::size() { return 1 + NumArgsSize(); }

void ANSend::list(ListingFile* listFile) {
  listFile->ListOp(*offset, op);
  ListNumArgs(listFile, *offset + 1, numArgs);
}

void ANSend::emit(FixupContext* fixup_ctxt, OutputFile* out) {
  out->WriteOp(op);
  WriteNumArgs(out, numArgs);
}

///////////////////////////////////////////////////
// Class ANSuper
///////////////////////////////////////////////////

ANSuper::ANSuper(std::string name, uint32_t c)
    : ANSend(op_super), classNum(c), name(std::move(name)) {
  if (classNum < 256) op |= OP_BYTE;
}

size_t ANSuper::size() { return (op & OP_BYTE ? 2 : 3) + NumArgsSize(); }

void ANSuper::list(ListingFile* listFile) {
  listFile->ListOp(*offset, op);
  listFile->ListArg("$%-4x\t(%s)", classNum, name);
  ListNumArgs(listFile, *offset + 1, numArgs);
}

void ANSuper::emit(FixupContext* fixup_ctxt, OutputFile* out) {
  out->WriteOp(op);
  if (op & OP_BYTE)
    out->WriteByte(classNum);
  else
    out->WriteWord(classNum);
  WriteNumArgs(out, numArgs);
}

///////////////////////////////////////////////////
// Class ANVars
///////////////////////////////////////////////////

ANVars::ANVars(VarList& theVars) : theVars(theVars) {}

size_t ANVars::size() { return 2 * (theVars.values.size() + 1); }

void ANVars::list(ListingFile* listFile) {
  // FIXME: I don't know why we're saving/restoring the variable.
  std::size_t curOfs = *offset;

  listFile->Listing("\n\nVariables:");
  listFile->ListWord(curOfs, theVars.values.size());
  curOfs += 2;

  for (Var const& var : theVars.values) {
    int n = var.value;
    if (var.type == S_STRING) n += *gTextTable->offset;
    listFile->ListWord(curOfs, n);
    curOfs += 2;
  }
  listFile->Listing("\n");
}

void ANVars::emit(FixupContext* fixup_ctxt, OutputFile* out) {
  std::size_t relOfs = 0;
  out->WriteWord(theVars.values.size());
  relOfs += 2;

  for (Var const& var : theVars.values) {
    int n = var.value;
    if (var.type == S_STRING) {
      n += *gTextTable->offset;
      fixup_ctxt->AddRelFixup(this, relOfs);
    }
    out->WriteWord(n);
    relOfs += 2;
  }
}

//////////////////////////////////////////////////////////////////////////////

ANFileName::ANFileName(std::string name) : ANOpCode(op_fileName), name(name) {}

void ANFileName::list(ListingFile* listFile) {
  switch (gConfig->targetArch) {
    case SciTargetArch::SCI_1_1:
      break;
    case SciTargetArch::SCI_2:
      listFile->ListOffset(*offset);
      listFile->Listing("file");
      break;

    default:
      throw std::runtime_error("Invalid target architecture");
  }
}

void ANFileName::emit(FixupContext* fixup_ctxt, OutputFile* out) {
  switch (gConfig->targetArch) {
    case SciTargetArch::SCI_1_1:
      break;
    case SciTargetArch::SCI_2:
      out->WriteOp(op);
      out->WriteNullTerminatedString(name);
      break;

    default:
      throw std::runtime_error("Invalid target architecture");
  }
}

size_t ANFileName::size() {
  switch (gConfig->targetArch) {
    case SciTargetArch::SCI_1_1:
      return 0;
    case SciTargetArch::SCI_2:
      return 1 + name.length() + 1;
    default:
      throw std::runtime_error("Invalid target architecture");
  }
}

////////////////////////////////////////////////////////////////////////////

ANLineNum::ANLineNum(int num) : ANOpCode(op_lineNum), num(num) {}

void ANLineNum::list(ListingFile* listFile) {
  switch (gConfig->targetArch) {
    case SciTargetArch::SCI_1_1:
      break;
    case SciTargetArch::SCI_2:
      listFile->ListSourceLine(num);
      break;

    default:
      throw std::runtime_error("Invalid target architecture");
  }
}

void ANLineNum::emit(FixupContext* fixup_ctxt, OutputFile* out) {
  switch (gConfig->targetArch) {
    case SciTargetArch::SCI_1_1:
      break;
    case SciTargetArch::SCI_2:
      out->WriteOp(op);
      out->WriteWord(num);
      break;

    default:
      throw std::runtime_error("Invalid target architecture");
  }
}

size_t ANLineNum::size() {
  switch (gConfig->targetArch) {
    case SciTargetArch::SCI_1_1:
      return 0;
    case SciTargetArch::SCI_2:
      return 1 + sizeof(SCIWord);

    default:
      throw std::runtime_error("Invalid target architecture");
  }
}
