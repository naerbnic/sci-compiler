// symtbl.hpp
//		definitions for symbol tables

#if !defined(SYMTBL_HPP)
#define SYMTBL_HPP

#if !defined(SYMBOL_HPP)
#include "symbol.hpp"
#endif

// Possible hash table sizes for symbol tables.
const int ST_LARGE		= 253;
const int ST_MEDIUM	= 89;
const int ST_SMALL		= 19;
const int ST_MINI		= 5;

class SymTbl {
// The SymTbl class is a collection of Symbols.  It is implemented as a hash
// table of user-selectable size (#defines for some sizes given above).
// A symbol to be added to the table has its name hashed by the hash() method,
// and the hash value is used to index into the hash table, which holds a
// pointer to the first element of a list of symbols which hash to that value.
// To keep the lists short, the table size should be larger for tables expected
// to hold more symbols.
//
// SymTbls are created globally, for each module, and for each method or
// procedure compiled.  They are linked into FIFO list called 'activeList' in
// order to implement block-structured scoping.  When the symbols in a table
// go out of scope (when we're done compiling a method, for example), they are
// moved to 'inactiveList' if they are needed to generate a listing later
// or are deleted if no listing has been requested.

public:
	Symbol*	add(Symbol* sym);
					// Add the Symbol 'sym' to this table.

	Symbol*	firstSym();
					// Return the first symbol in the table, or 0 if the table
					// is empty.

	Symbol*	lookup(strptr name);
					// Return a pointer to the symbol with name 'name' if it is in
					// this table, 0 otherwise.

	Symbol*	nextSym();
					// Return the next symbol in the table, or 0 if at the end
					// of the table.

private:
	SymTbl(int size = ST_MEDIUM, Bool retain = False);
	~SymTbl();

	Symbol*	install(strptr name, sym_t type);
					// Install the identifier 'name' as a Symbol of type 'type'.
					// Return a pointer to it.

	Symbol*	remove(strptr name);
					// Remove the Symbol with name 'name' from this table.  Return
					// a pointer to the Symbol.

	Bool		del(strptr name);
					// Remove and delete a Symbol with name 'name' from the table.
					// Return True if it was found and deleted, False otherwise.

	void		retain(Bool keepIt = True) { keep = keepIt; }
					// Set the 'keep' property to the value of 'keepIt'

	void		clearAsmPtrs();
					// Make sure that all pointers in symbols in the symbol table
					//	refering to ANodes is cleared.

	SymTbl*	next;				// pointer to next symbol table
	int		hashSize;		// size of hash table
	Symbol**	hashTable;		// pointer to the hash table
	Bool		keep;	// should this table be kept for listings when out of scope?
	Symbol*	curSym;			// current Symbol in firstSym()/nextSym()
	int		curIndex;		// current hash table index in firstSym()/nextSym()

	uint		hash(strptr str);
					// Return the hash value of the string for this table

	friend class SymTbls;
};

class SymTbls {
public:
	SymTbls();

	SymTbl*	add(int size = ST_MEDIUM, Bool retain = False);
					//	add a New symbol table and link it into the list

	void		clearAsmPtrs();

	void		deactivate(SymTbl*);
				// Remove this table from the active SymTbl list ('activeList').
				// Delete it if not necessary for the listing, add it to
				// 'inactiveList' otherwise.

	Bool		del(strptr name);
				// Delete the symbol with name 'name' from the SymTbls in activeList
				// Return True if the symbol was present, False otherwise.

	Symbol*	installLocal(strptr n, sym_t t)
					{ return activeList->install(n, t); }
	Symbol*	installModule(strptr n, sym_t t)
					{ return moduleSymTbl->install(n, t); }
	Symbol*	installGlobal(strptr n, sym_t t)
					{ return globalSymTbl->install(n, t); }
	Symbol*	installClass(strptr n)
					{ return classSymTbl->install(n, S_CLASS); }
	Symbol*	installSelector(strptr n)
					{ return selectorSymTbl->install(n, S_SELECT); }

	void		delFreeTbls();
				// Delete all symbol tables which do not have their 'keep' property
				//	set

	Symbol*	lookup(strptr name);
				// syms.lookup the Symbol with name 'name' in 'activeList'. Return 0
				// if not found.

	Symbol*	remove(strptr name);
				// Remove the symbol with name 'name' from the SymTbls in activeList
				// Return a pointer to the symbol if found, NULL otherwise.

	SymTbl*	classSymTbl;
	SymTbl*	selectorSymTbl;
	SymTbl*	moduleSymTbl;

private:
	void		unlink(SymTbl*);
				// Unlink this table from the active symbol table list.

	SymTbl*	activeList;
	SymTbl*	inactiveList;

	SymTbl*	globalSymTbl;

} extern syms;

#endif
