//	anode.cpp
// 	assemble an object code list

#include <stdlib.h>
#include <string.h>

#include "sol.hpp"

#include "string.hpp"

#include	"sc.hpp"

#include	"anode.hpp"
#include	"asm.hpp"
#include	"define.hpp"
#include	"error.hpp"
#include	"listing.hpp"
#include	"opcodes.hpp"
#include	"object.hpp"
#include	"optimize.hpp"
#include	"symbol.hpp"
#include	"text.hpp"
#include	"output.hpp"

ANCodeBlk*	codeStart;
size_t		curOfs;
int			textStart;

#define OPTIMIZE_TRANSFERS

///////////////////////////////////////////////////
// Class ANReference
///////////////////////////////////////////////////

ANReference::ANReference() : target(0), sym(0)
{
}

void
ANReference::addBackpatch(
	Symbol* sym)
{
	backLink = sym->ref;
	sym->ref = this;
}

void
ANReference::backpatch(
	ANode*	dest)
{
   //	warning:  backLink and target are unioned
	ANReference* next = backLink;
	target = dest;
	if (next)
		next->backpatch(dest);
}

///////////////////////////////////////////////////
// Class ANode
///////////////////////////////////////////////////

// The flag addNodesToList is set during the optimization phase so that New
// nodes to replace old ones are not automatically added to the current list.

ANode::ANode(
	AList*	list) : offset(0)
{
	next = 0;
	prev = 0;
	if (addNodesToList && list)
		list->add(this);
}

ANode::ANode(
	AList*	list,
	ANode*	before) : offset(0)
{
	next = 0;
	prev = 0;
	if (addNodesToList && list)
		list->addBefore(before, this);
}

size_t
ANode::size()
{
	return 0;
}

size_t
ANode::setOffset(
	size_t ofs)
{
	offset = ofs;
	return ofs + size();
}

void
ANode::emit(OutputFile*)
{
}

void
ANode::list()
{
}

Bool
ANode::optimize()
{	
	return False;
}

///////////////////////////////////////////////////
// Class ANDispatch
///////////////////////////////////////////////////

void
ANDispatch::list()
{
	size_t	oldOfs = curOfs;

	if (target && sym)
		ListAsCode("dispatch\t$%-4x\t(%s)", target->offset, sym->name);
	else if (sym)
		ListAsCode("dispatch\t----\t(%s)", sym->name);
	else
		ListAsCode("dispatch\t----");

	curOfs = oldOfs;
}

size_t
ANDispatch::size()
{
	return 2;
}

void
ANDispatch::emit(OutputFile* out)
{
	// If the destination of this dispatch entry is in the heap (i.e.
	// an object rather than code), it must be fixed up.

	if (sc->heapList->contains(target))
		sc->hunkList->addFixup(offset);

	out->WriteWord((uint) (target && sym ? target->offset : 0));
}

void
ANDispatch::backpatch(
	ANode* dest)
{
	if (sc->heapList->contains(dest))
		sc->hunkList->incFixups();

	ANReference::backpatch(dest);
}

///////////////////////////////////////////////////
// Class ANWord
///////////////////////////////////////////////////

ANWord::ANWord(
	int v) : value(v)
{
}

ANWord::ANWord(
	AList*	list,
	int		v) : ANode(list),  value(v)
{
}

size_t
ANWord::size()
{
	return 2;
}

void
ANWord::list()
{
	ListWord(value);
}

void
ANWord::emit(OutputFile* out)
{
	out->WriteWord(value);
}

///////////////////////////////////////////////////
// Class ANTable
///////////////////////////////////////////////////

ANTable::ANTable(
	strptr	nameStr,
	ANode*	before) :
	
	ANode(curList, before), name(nameStr), oldList(curList)
{
	curList = &entries;
}

size_t
ANTable::size()
{
	return entries.size();
}

size_t
ANTable::setOffset(
	size_t ofs)
{
	offset = ofs;
	return entries.setOffset(ofs);
}

void
ANTable::list()
{
	Listing("\t\t(%s)", name);
}

void
ANTable::emit(OutputFile* out)
{
	entries.emit(out);
}

void
ANTable::finish()
{
	curList = oldList;
}

///////////////////////////////////////////////////
// Class ANObjTable
///////////////////////////////////////////////////

ANObjTable::ANObjTable(
	strptr	nameStr) :

	ANTable(nameStr, codeStart)
{
}

///////////////////////////////////////////////////
// Class ANText
///////////////////////////////////////////////////

ANText::ANText(
	Text*	tp) : ANode(sc->heapList), text(tp)
{
}

size_t
ANText::setOffset(
	size_t ofs)
{
	if (!textStart)
		textStart = ofs;
	return ANode::setOffset(ofs);
}

size_t
ANText::size()
{
	return strlen(text->str) + 1;
}

void
ANText::list()
{
	if (textStart == offset)
		Listing("\n\n");
	ListText(text->str);
}

void
ANText::emit(OutputFile* out)
{
	out->Write(text->str, size());
}

///////////////////////////////////////////////////
// Class ANObject
///////////////////////////////////////////////////

ANObject::ANObject(
	Symbol*	s,
	int		n,
	ANode*	before) : ANode(curList, before), sym(s), num(n)
{
}

void
ANObject::list()
{
	Listing("\nObject: %-20s", sym->name);
}

///////////////////////////////////////////////////
// Class ANCodeBlk
///////////////////////////////////////////////////

ANCodeBlk::ANCodeBlk(
	Symbol*	s) : sym(s)
{
	ANLabel::reset();
	oldList = curList;
	curList = &code;

	if (!codeStart)
		codeStart = this;
}

size_t
ANCodeBlk::size()
{
	return code.size();
}

void
ANCodeBlk::emit(OutputFile* out)
{
	code.emit(out);
}

size_t
ANCodeBlk::setOffset(
	size_t ofs)
{
	offset = ofs;
	return code.setOffset(ofs);
}

void
ANCodeBlk::finish()
{
	curList = oldList;
}

Bool
ANCodeBlk::optimize()
{
	return OptimizeProc(&code);
}

///////////////////////////////////////////////////
// Class ANProcCode
///////////////////////////////////////////////////

void
ANProcCode::list()
{
	Listing("\n\nProcedure: (%s)\n", sym->name);
}

///////////////////////////////////////////////////
// Class ANMethCode
///////////////////////////////////////////////////

ANMethCode::ANMethCode(
	Symbol*	s) : ANCodeBlk(s), objSym(curObj->sym)
{
}

void
ANMethCode::list()
{
	Listing("\n\nMethod: (%s %s)\n", objSym->name, sym->name);
}

///////////////////////////////////////////////////
// Class ANProp
///////////////////////////////////////////////////

ANProp::ANProp(
	Symbol*	sp,
	int		v) : sym(sp), val(v)
{
}

size_t
ANProp::size()
{
	return 2;
}

void
ANProp::list()
{
	ListAsCode("%-6s$%-4x\t(%s)", desc(), (SCIUWord) value(), sym->name);
}

void
ANProp::emit(OutputFile* out)
{
	out->WriteWord(value());
}

strptr
ANIntProp::desc()
{	
	return "prop";
}

uint
ANIntProp::value()
{
	return val;
}

ANTextProp::ANTextProp(
	Symbol*	sp,
	int		v) : ANProp(sp, v)
{
	sc->heapList->incFixups();
}

void
ANTextProp::emit(OutputFile* out)
{
	sc->heapList->addFixup(offset);
	ANProp::emit(out);
}

strptr
ANTextProp::desc()
{
	return "text";
}

uint
ANTextProp::value()
{
	return val + textStart;
}

strptr
ANOfsProp::desc()
{
	return "ofs";
}

uint
ANOfsProp::value()
{
	return target->offset;
}

ANMethod::ANMethod(
	Symbol*		sp,
	ANMethCode*	mp) : ANProp(sp, 0), method(mp)
{
}

strptr
ANMethod::desc()
{
	return "local";
}

uint
ANMethod::value()
{	
	return method->offset;
}

///////////////////////////////////////////////////
// Class ANOpCode
///////////////////////////////////////////////////

ANOpCode::ANOpCode(uint o) : op(o)
{
}

size_t
ANOpCode::size()
{
	return 1;
}

void
ANOpCode::list()
{
	ListOp(op);
}

void
ANOpCode::emit(OutputFile* out)
{
	out->WriteOp(op);
}

///////////////////////////////////////////////////
// Class ANLabel
///////////////////////////////////////////////////

uint	ANLabel::nextLabel = 0;

ANLabel::ANLabel() : ANOpCode(OP_LABEL), number(nextLabel++)
{
}

size_t
ANLabel::size()
{
	return 0;
}

void
ANLabel::list()
{
	Listing(".%d", number);
}

void
ANLabel::emit(OutputFile*)
{
}

///////////////////////////////////////////////////
// Class ANOpUnsign
///////////////////////////////////////////////////

ANOpUnsign::ANOpUnsign(
	uint o,
	uint v)
{
	value = v;
#if defined(OPTIMIZE_TRANSFERS)
	op = o | (value < 256 ? OP_BYTE : 0);
#else
	if (o == op_link || o == op_class)
		op = o | (value < 256 ? OP_BYTE : 0);
	else 
		op = o | (value < 128 ? OP_BYTE : 0);
#endif
	sym = 0;
}

size_t
ANOpUnsign::size()
{
	return op & OP_BYTE ? 2 : 3;
}

void
ANOpUnsign::list()
{
	ListOp(op);
	if (!sym)
		ListArg("$%-4x", (SCIUWord) value);
	else
		ListArg("$%-4x\t(%s)", (SCIUWord) value, sym->name ? sym->name : "");
}

void
ANOpUnsign::emit(OutputFile* out)
{
	out->WriteOp(op);
	if (op & OP_BYTE)
		out->WriteByte(value);
	else
		out->WriteWord(value);
}

///////////////////////////////////////////////////
// Class ANOpSign
///////////////////////////////////////////////////

ANOpSign::ANOpSign(
	uint	o,
	int	v)
{
	value = v;
	op = o | ((uint) abs(value) < 128 ? OP_BYTE : 0);
	sym = 0;
}

size_t
ANOpSign::size()
{
	return op & OP_BYTE ? 2 : 3;
}

void
ANOpSign::list()
{
	ListOp(op);
	if (!sym)
		ListArg("$%-4x", (SCIUWord) value);
	else
		ListArg("$%-4x\t(%s)", (SCIUWord) value, sym->name ? sym->name : "");
}

void
ANOpSign::emit(OutputFile* out)
{
	out->WriteOp(op);
	if (op & OP_BYTE)
		out->WriteByte(value);
	else
		out->WriteWord(value);
}

///////////////////////////////////////////////////
// Class ANOpExtern
///////////////////////////////////////////////////

ANOpExtern::ANOpExtern(
	Symbol*	s,
	uint		m,
	uint		e) : sym(s), module(m), entry(e)
{
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

size_t
ANOpExtern::size()
{
	switch (op & ~OP_BYTE) {
		case op_callk:
		case op_callb:
			return op & OP_BYTE ? 4 : 5;
		case op_calle:
			return op & OP_BYTE ? 5 : 7;
		default:
			return 0;
		}
}

void
ANOpExtern::list()
{
	ListOp(op);
	switch (op & ~OP_BYTE) {
		case op_callk:
		case op_callb:
			ListArg("$%-4x\t(%s)", (SCIUWord) entry, sym->name);
			break;
		case op_calle:
			ListArg("$%x/%x\t(%s)", (SCIUWord) module, (SCIUWord) entry,
				sym->name);
		}
	ListWord(numArgs);
}

void
ANOpExtern::emit(OutputFile* out)
{
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

	out->WriteWord(numArgs);
}

///////////////////////////////////////////////////
// Class ANCall
///////////////////////////////////////////////////

ANCall::ANCall(
	Symbol*	s)
{
	sym = s;
	op = op_call;
	offset = curOfs;
}

size_t
ANCall::size()
{
	if (!shrink)
		return op & OP_BYTE ? 4 : 5;
	else if (!sym->loc || target->offset == UNDEFINED)
		return 5;
#if	defined(OPTIMIZE_TRANSFERS)
	else if (abs(target->offset - (offset + 5)) < 128) {
		op |= OP_BYTE;
		return 4;
	}
#endif
	else {
		op &= ~OP_BYTE;
		return 5;
	}
}

void
ANCall::list()
{
	ListOp(op_call);
	ListArg("$%-4x\t(%s)", SCIUWord(target->offset - (offset + size())),
		sym->name);
	ListWord(numArgs);
}

void
ANCall::emit(OutputFile* out)
{
	if (!target || target->offset == UNDEFINED) {
		Error("Undefined procedure: %s", sym->name);
		return;
	}

	int n = target->offset - (offset + size());
	out->WriteOp(op);
	if (op & OP_BYTE)
		out->WriteByte(n);
	else
		out->WriteWord(n);

	out->WriteWord(numArgs);
}

///////////////////////////////////////////////////
// Class ANBranch
///////////////////////////////////////////////////

ANBranch::ANBranch(
	uint o)
{
	op = o;
}

size_t
ANBranch::size()
{
	if (!shrink)
		return op & OP_BYTE ? 2 : 3;
	else if (!target || target->offset == UNDEFINED)
		return 3;
#if	defined(OPTIMIZE_TRANSFERS)
	else if (abs(target->offset - (offset + 4)) < 128) {
		op |= OP_BYTE;
		return 2;
	}
#endif
	else {
		op &= ~OP_BYTE;
		return 3;
	}
}

void
ANBranch::list()
{
	ListOp(op);
	ListArg("$%-4x\t(.%d)", SCIUWord(target->offset - (offset + size())),
		((ANLabel *) target)->number);
}

void
ANBranch::emit(OutputFile* out)
{
	int n = target->offset - (offset + size());

	out->WriteOp(op);
	if (op & OP_BYTE)
		out->WriteByte(n);
	else
		out->WriteWord(n);
}

///////////////////////////////////////////////////
// Class ANVarAccess
///////////////////////////////////////////////////

ANVarAccess::ANVarAccess(
	uint	o,
	uint	a) : addr(a)
{
	op = addr < 256 ? o | OP_BYTE : o;
}

size_t
ANVarAccess::size()
{
	return op & OP_BYTE ? 2 : 3;
}

void
ANVarAccess::list()
{
	ListOp(op);
	if (sym)
		ListArg("$%-4x\t(%s)", addr, sym->name ? sym->name : "");
	else
		ListArg("$%-4x", addr);
}

void
ANVarAccess::emit(OutputFile* out)
{
	out->WriteOp(op);
	if (op & OP_BYTE)
		out->WriteByte(addr);
	else
		out->WriteWord(addr);
}

///////////////////////////////////////////////////
// Class ANOpOfs
///////////////////////////////////////////////////

ANOpOfs::ANOpOfs(
	uint	o) : ANOpCode(op_lofsa), ofs(o)
{
	sc->hunkList->incFixups();
}

size_t
ANOpOfs::size()
{
	return WORDSIZE;
}

void
ANOpOfs::list()
{
	ListOp(op);
	ListArg("$%-4x", textStart + ofs);
}

void
ANOpOfs::emit(OutputFile* out)
{
	out->WriteOp(op);
	sc->hunkList->addFixup(offset + 1);
	out->WriteWord(textStart + ofs);
}

///////////////////////////////////////////////////
// Class ANObjID
///////////////////////////////////////////////////

ANObjID::ANObjID(
	Symbol*	s) : ANOpCode(op_lofsa)
{
	sym = s;
	sc->hunkList->incFixups();
}

size_t
ANObjID::size()
{
	return WORDSIZE;
}

void
ANObjID::list()
{
	ListOp(op);
	ListArg("$%-4x\t(%s)", target->offset, sym->name);
}

void
ANObjID::emit(OutputFile* out)
{
	if (!sym->obj) {
		Error("Undefined object from line %u: %s", sym->lineNum, sym->name);
		return;
	}

	out->WriteOp(op);
	sc->hunkList->addFixup(offset+1);
	out->WriteWord(target->offset);
}

///////////////////////////////////////////////////
// Class ANEffctAddr
///////////////////////////////////////////////////

ANEffctAddr::ANEffctAddr(
	uint	o,
	uint	a,
	uint	t) : ANVarAccess(o, a), eaType(t)
{
}

size_t
ANEffctAddr::size()
{
	return op & OP_BYTE ? 3 : 5;
}

void
ANEffctAddr::list()
{
	ListOp(op);
	ListArg("$%-4x\t(%s)", addr, sym->name);
}

void
ANEffctAddr::emit(OutputFile* out)
{
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

ANSend::ANSend(
	uint	o) : ANOpCode(o)
{
}

size_t
ANSend::size()
{
	return 3;
}

void
ANSend::list()
{
	ListOp(op);
	ListWord(numArgs);
}

void
ANSend::emit(OutputFile* out)
{
	out->WriteOp(op);
	out->WriteWord(numArgs);
}

///////////////////////////////////////////////////
// Class ANSuper
///////////////////////////////////////////////////

ANSuper::ANSuper(
	Symbol*	s,
	uint		c) : ANSend(op_super), sym(s), classNum(c)
{
	if (classNum < 256)
		op |= OP_BYTE;
}

size_t
ANSuper::size()
{
	return op & OP_BYTE ? 4 : 5;
}

void
ANSuper::list()
{
	ListOp(op);
	ListArg("$%-4x\t(%s)", classNum, sym->name);
	ListWord(numArgs);
}

void
ANSuper::emit(OutputFile* out)
{
	out->WriteOp(op);
	if (op & OP_BYTE)
		out->WriteByte(classNum);
	else
		out->WriteWord(classNum);
	out->WriteWord(numArgs);
}

///////////////////////////////////////////////////
// Class ANVars
///////////////////////////////////////////////////

ANVars::ANVars(
	VarList&	theVars) : ANode(0), theVars(theVars)
{
	sc->heapList->addAfter(sc->heapList->first(), this);
	sc->heapList->incFixups(theVars.fixups);
}

size_t
ANVars::size()
{
	return 2 * (theVars.size + 1);
}

void
ANVars::list()
{
	size_t oldOfs = curOfs;

	Listing("\n\nVariables:");
	ListWord(theVars.size);
	curOfs += 2;

	Var* vp = theVars.values;
	for (int i = 0; i < theVars.size; ++i, ++vp) {
		int n = vp->value;
		if (vp->type == S_STRING)
			n += textStart;
		ListWord(n);
		curOfs += 2;
	}
	Listing("\n");

	curOfs = oldOfs;
}

void
ANVars::emit(OutputFile* out)
{
	out->WriteWord(theVars.size);
	curOfs += 2;

	Var* vp = theVars.values;
	for (int i = 0 ; i < theVars.size ; ++i, ++vp) {
		int n = vp->value;
		if (vp->type == S_STRING) {
			n += textStart;
			sc->heapList->addFixup(curOfs);
		}
		out->WriteWord(n);
		curOfs += 2;
	}
	theVars.kill();
}

//////////////////////////////////////////////////////////////////////////////

ANFileName::ANFileName(const char* name) :
	ANOpCode(op_fileName),
	name(strdup ( name ))
{
}

ANFileName::~ANFileName()
{
	free ( (void *)name );
}

void
ANFileName::list()
{
	ListOffset();
	Listing("file");
}

void
ANFileName::emit(OutputFile* out)
{
	out->WriteOp(op);
	out->Write(name, strlen(name) + 1);
}

size_t
ANFileName::size()
{
	return 1 + strlen(name) + 1;
}

////////////////////////////////////////////////////////////////////////////

ANLineNum::ANLineNum(int num) :
	ANOpCode(op_lineNum),
	num(num)
{
}

void
ANLineNum::list()
{
	ListSourceLine(num);
}

void
ANLineNum::emit(OutputFile* out)
{
	out->WriteOp(op);
	out->WriteWord(num);
}

size_t
ANLineNum::size()
{
	return 1 + sizeof(SCIWord);
}

