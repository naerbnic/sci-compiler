//	alist.cpp

#include "sol.hpp"

#include	"sc.hpp"

#include "alist.hpp"
#include	"anode.hpp"
#include	"listing.hpp"
#include	"opcodes.hpp"
#include	"output.hpp"

AList*	curList;
Bool		shrink;
Bool		noOptimize;

///////////////////////////////////////////////////
// Class AList
///////////////////////////////////////////////////

ANOpCode*
AList::nextOp(
	ANode*	start)
{
	assert(start != NULL);

	ANOpCode *nn;

	for (nn = (ANOpCode *) start->next;
		  nn && nn->op == OP_LABEL;
		  nn = (ANOpCode *) nn->next)
		;

	return nn;
}

ANOpCode*
AList::findOp(
	uint op)
{
	ANOpCode* nn = (ANOpCode *) cur->next;

    if ( nn )
    	return nn->op == op ? nn : 0;
    else
        return 0;
}

Bool
AList::removeOp(
	uint op)
{
	ANode* an = findOp(op);
	if (an)
		del(an);

	return an != 0;
}

size_t
AList::size()
{
	size_t	s = 0;
	for (ANode* an = (ANode*) first(); an; an = (ANode*) next())
		s += an->size();
	return s;
}

void
AList::emit(OutputFile* out)
{
	for (ANode* an = (ANode*) first(); an; an = (ANode*) next()) {
		curOfs = an->offset;
		if (listCode)
			an->list();
		an->emit(out);
	}
}

size_t
AList::setOffset(
	size_t ofs)
{
	for (ANode* an = (ANode*) first(); an; an = (ANode*) next())
		ofs = an->setOffset(ofs);

	return ofs;
}

void
AList::optimize()
{
	// Scan the assembly nodes, doing some peephole-like optimization.

	if (noOptimize)
		return;

	for (ANode* an = (ANode*) first(); an; an = (ANode*) next())
		while (an->optimize())
			;
}

///////////////////////////////////////////////////
// Class FixupList
///////////////////////////////////////////////////

FixupList::FixupList() : fixups(0), numFixups(0), fixIndex(0)
{
}

FixupList::~FixupList()
{
	clear();
}

void
FixupList::clear()
{
	AList::clear();
	delete[] fixups;

	fixups = 0;
	numFixups = fixIndex = 0;

	// All fixup lists have a word node at the start which is the offset
	// to the fixup table.
	New ANWord(this, 0);
}

size_t
FixupList::setOffset(
	size_t ofs)
{
	fixOfs = AList::setOffset(ofs);
	return fixOfs;
}

void
FixupList::initFixups()
{
	// Set offset to fixup table.  If the table is on an odd boundary,
	// adjust to an even one.

	((ANWord *) head)->value = fixOfs + (fixOfs & 1);

	fixups = new size_t[numFixups];
	fixIndex = 0;
}

void
FixupList::listFixups()
{
	curOfs = fixOfs;

	if (curOfs & 1) {
		ListByte(0);
		++curOfs;
	}

	Listing("\n\nFixups:");
	ListWord(numFixups);
	curOfs += 2;

	for (int i = 0; i < numFixups; ++i) {
		ListWord(fixups[i]);
		curOfs += 2;
	}
}

void
FixupList::emitFixups(OutputFile* out)
{
	if (listCode)
		listFixups();

	if (fixOfs & 1)
		out->WriteByte(0);

	out->WriteWord(numFixups);
	for (int i = 0; i < numFixups; ++i)
		out->WriteWord(fixups[i]);
}

void
FixupList::addFixup(
	size_t ofs)
{
	fixups[fixIndex++] = ofs;
}

void
FixupList::emit(OutputFile* out)
{
	initFixups();
	AList::emit(out);
	emitFixups(out);
}

///////////////////////////////////////////////////
// Class CodeList
///////////////////////////////////////////////////

void
CodeList::optimize()
{
	AList::optimize();

	// Make a first pass, resolving offsets and converting to byte offsets
	// where possible.
	shrink = True;
	setOffset(0);

	// Continue resolving and converting to byte offsets until we've shrunk
	// the code as far as it will go.
	size_t	curLen = 0, oldLen;
	do {
		oldLen = curLen;
		curLen = setOffset(0);
	} while (oldLen > curLen);

	// Now stabilize the code and offsets by resolving without allowing
	// conversion to byte offsets.
	shrink = False;
	do {
		oldLen = curLen;
		curLen = setOffset(0);
	} while (oldLen != curLen);
}

