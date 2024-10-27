//	symtbl.cpp	sc
// 	symbol table routines for sc

#include <string.h>

#include "sol.hpp"

#include	"sc.hpp"

#include	"listing.hpp"
#include	"object.hpp"
#include	"symtbl.hpp"

SymTbls	syms;

SymTbl::SymTbl(
	int 	size,
	Bool	keep) :

	hashSize(size), keep(keep)
{
	hashTable = New Symbol*[size];
	memset(hashTable, 0, size * sizeof(Symbol*));
}

SymTbl::~SymTbl()
{
	Symbol*	sp;
	Symbol*	nextSym;
	
	// Free each piece of the table.
	for (int i = 0; i < hashSize; ++i) {
		for (sp = hashTable[i]; sp; sp = nextSym) {
			nextSym = sp->next;
			delete sp;
		}
	}

	delete[] hashTable;
}

void
SymTbl::clearAsmPtrs()
{
	// Make sure that all pointers to assembly nodes (the an element of the
	// structure) are cleared in a symbol table.

	for (Symbol* sym = firstSym(); sym; sym = nextSym())
		sym->an = 0;
}

Symbol *
SymTbl::install(
	strptr	name,
	sym_t		type)
{
	// Install the identifier with name 'name' in the symbol table 'symTbl'
	// with type 'type'.

	return add(New Symbol(name, type));
}

Symbol*
SymTbl::add(
	Symbol*	sp)
{
	// Get the hash value for the symbol in this hash table.
	uint hashVal = hash(sp->name);

	// Link the symbol in at the beginning of the appropriate hash list
	sp->next = hashTable[hashVal];
	hashTable[hashVal] = sp;

	return sp;
}

Symbol *
SymTbl::lookup(
	strptr name)
{
	// Search this symbol table for the symbol whose name is pointed to by
	// 'name'.  Return a pointer to the symbol if found, NULL otherwise.
	// Note that when a symbol is found, it is moved to the front of its list,
	// on the assumption that references to a symbol will tend to be clustered.
	// If nothing else, this puts those symbols which are not used at all at the
	// end of the list.

	Symbol*	prev = 0;
	Symbol**	start = &hashTable[hash(name)];
	for (Symbol* sp = *start; sp; sp = sp->next) {
		if (!strcmp(name, sp->name)) {
			// Move the symbol to the start of the list.
			if (prev) {
				prev->next = sp->next;
				sp->next = *start;
				*start = sp;
			}
			return sp;
		}
		prev = sp;
	}

	return 0;
}

Symbol*
SymTbl::remove(
	strptr name)
{
	// Try to remove the symbol with name pointed to by 'name' from this table
	// and return a pointer to it if successful, NULL otherwise.

	Symbol*	prev = 0;
	Symbol**	start = &hashTable[hash(name)];
	for (Symbol* sp = *start; sp; sp = sp->next) {
		if (!strcmp(name, sp->name)) {
			// Link around symbol and delete it.
			if (!prev)
				*start = sp->next;
			else 
				prev->next = sp->next;
			return sp;
		}
		prev = sp;
	}

	return 0;
}

Bool
SymTbl::del(
	strptr name)
{
	// Delete the symbol with name pointed to by 'name' from this table and
	// return True if successful, False otherwise.

	Symbol *sp = remove(name);
	delete sp;

	return (Bool) sp;
}

uint
SymTbl::hash(
	strptr str)
{
	// Compute the hash value of the string 'str'.
	long value;
	for (value = 0 ; *str; value += *str++)
		;

	return (int) (value % (long) hashSize);
}

Symbol*
SymTbl::firstSym()
{
	// Open a symbol table for sequential access to its symbols using
	//	nextSym().  Return the first symbol in the table, or 0 if the table 
	// is empty.

	curIndex = -1;
	curSym = 0;
	return nextSym();
}

Symbol*
SymTbl::nextSym()
{
	// Point to the next symbol in the current hash list.
	if (curSym)
		curSym = curSym->next;

	// If we're not pointing at a symbol, search for the next list
	// in the hash chain which has one.
	while (!curSym && ++curIndex < hashSize)
		curSym = hashTable[curIndex];

	return curSym;
}

////////////////////////////////////////////////////////////////////////////

SymTbls::SymTbls() :

	activeList(0), inactiveList(0)
{
	moduleSymTbl = add(ST_MEDIUM, False);
	selectorSymTbl = add(ST_MEDIUM, True);
	classSymTbl = add(ST_SMALL, True);
	globalSymTbl = add(ST_LARGE, True);
}

SymTbl*
SymTbls::add(
	int	size,
	Bool	keep)
{
	// Add a New symbol table to the front of the active list

	SymTbl* sp = New SymTbl(size, keep);
	sp->next = activeList;
	activeList = sp;
	return sp;
}


void
SymTbls::clearAsmPtrs()
{
	selectorSymTbl->clearAsmPtrs();
	classSymTbl->clearAsmPtrs();
	globalSymTbl->clearAsmPtrs();
}

Symbol*
SymTbls::lookup(
	strptr name)
{
	// Search the active symbol tables for the symbol whose name is pointed
	// to by 'name'.  Return a pointer to the symbol if found, NULL otherwise.

	for (SymTbl* tp = activeList; tp; tp = tp->next) {
		Symbol* sp = tp->lookup(name);
		if (sp)
			return sp;
	}

	return 0;
}

void
SymTbls::delFreeTbls()
{
	// Delete all symbol tables which do not have their 'keep' flag set.

	SymTbl*	sp;
	SymTbl*	ptr;

	for (sp = activeList; sp; sp = ptr) {
		ptr = sp->next;
		if (!sp->keep) {
			unlink(sp);
			delete sp;
		}
	}

	// Delete inactive symbol tables.
	for (sp = inactiveList; sp; sp = ptr) {
		ptr = sp->next;
		delete sp;
	}
	inactiveList = 0;
}

Bool
SymTbls::del(
	strptr name)
{
	// Search the active symbol tables for the symbol whose name
	// is pointed to by 'name'.  If it is found, delete the symbol
	// and return True, else return False.

	for (SymTbl* tp = activeList; tp; tp = tp->next)
		if (tp->del(name))
			return True;

	return False;
}

Symbol*
SymTbls::remove(
	strptr name)
{
	// Search the active symbol tables for the symbol whose name is pointed
	// to by 'name'.  Remove it and return a pointer to the symbol if found,
	// return 0 otherwise.

	for (SymTbl* tp = activeList; tp; tp = tp->next) {
		Symbol* sp = tp->remove(name);
		if (sp)
			return sp;
	}

	return 0;
}

void
SymTbls::deactivate(
	SymTbl*	tbl)
{
	// Add this symbol table to the inactive list.

	// Remove the symbol table from the symbol table list.
	unlink(tbl);

	if (!listCode)
		delete tbl;

	else {
		// See if the table is already in the inactive list.
		SymTbl* tp;
		for (tp = inactiveList; tp && tp != tbl; tp = tp->next)
			;

		// If the table is not in the inactive list, add it.
		if (!tp) {
			tbl->next = inactiveList;
			inactiveList = tbl;
		}
	}
}

void
SymTbls::unlink(
	SymTbl*	tbl)
{
	// Unlink this symbol table from the active list.

	if (tbl == activeList)
		activeList = tbl->next;

	else {
		// Search for the symbol table preceeding this one in the list.
		SymTbl* tp;
		for (tp = activeList; tp && tp->next != tbl; tp = tp->next)
			;

		// Link around this table.
		if (tp)
			tp->next = tbl->next;
	}
}
