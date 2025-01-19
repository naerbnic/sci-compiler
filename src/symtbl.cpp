//	symtbl.cpp	sc
// 	symbol table routines for sc

#include "symtbl.hpp"

#include <string.h>

#include "listing.hpp"
#include "object.hpp"
#include "sc.hpp"
#include "sol.hpp"

SymTbls syms;

SymTbl::SymTbl(int size, bool keep) : keep(keep) {}

SymTbl::~SymTbl() {}

void SymTbl::clearAsmPtrs() {
  // Make sure that all pointers to assembly nodes (the an element of the
  // structure) are cleared in a symbol table.

  for (Symbol* sym = firstSym(); sym; sym = nextSym()) sym->clearAn();
}

Symbol* SymTbl::install(strptr name, sym_t type) {
  // Install the identifier with name 'name' in the symbol table 'symTbl'
  // with type 'type'.

  return add(new Symbol(name, type));
}

Symbol* SymTbl::add(Symbol* sp) {
  // Take ownership of the symbol
  symbols.emplace(sp->name(), std::unique_ptr<Symbol>(sp));

  return sp;
}

Symbol* SymTbl::lookup(strptr name) {
  // Search this symbol table for the symbol whose name is pointed to by
  // 'name'.  Return a pointer to the symbol if found, NULL otherwise.
  // Note that when a symbol is found, it is moved to the front of its list,
  // on the assumption that references to a symbol will tend to be clustered.
  // If nothing else, this puts those symbols which are not used at all at the
  // end of the list.

  std::string_view name_view = name;
  auto it = symbols.find(name_view);
  if (it != symbols.end()) {
    return it->second.get();
  }

  return nullptr;
}

Symbol* SymTbl::remove(strptr name) {
  // Try to remove the symbol with name pointed to by 'name' from this table
  // and return a pointer to it if successful, NULL otherwise.

  auto it = symbols.find(std::string_view(name));

  if (it != symbols.end()) {
    Symbol* sp = it->second.release();
    symbols.erase(it);
    return sp;
  }

  return nullptr;
}

bool SymTbl::del(strptr name) {
  // Delete the symbol with name pointed to by 'name' from this table and
  // return True if successful, False otherwise.

  return symbols.erase(std::string_view(name)) > 0;
}

Symbol* SymTbl::firstSym() {
  // Open a symbol table for sequential access to its symbols using
  //	nextSym().  Return the first symbol in the table, or 0 if the table
  // is empty.
  curSym = symbols.begin();
  if (curSym == symbols.end()) return nullptr;
  return curSym->second.get();
}

Symbol* SymTbl::nextSym() {
  if (curSym == symbols.end()) return nullptr;

  ++curSym;

  if (curSym == symbols.end()) return nullptr;

  return curSym->second.get();
}

////////////////////////////////////////////////////////////////////////////

SymTbls::SymTbls()
    :

      activeList(0),
      inactiveList(0) {
  moduleSymTbl = add(ST_MEDIUM, False);
  selectorSymTbl = add(ST_MEDIUM, True);
  classSymTbl = add(ST_SMALL, True);
  globalSymTbl = add(ST_LARGE, True);
}

SymTbl* SymTbls::add(int size, bool keep) {
  // Add a new symbol table to the front of the active list

  SymTbl* sp = new SymTbl(size, keep);
  sp->next = activeList;
  activeList = sp;
  return sp;
}

void SymTbls::clearAsmPtrs() {
  selectorSymTbl->clearAsmPtrs();
  classSymTbl->clearAsmPtrs();
  globalSymTbl->clearAsmPtrs();
}

Symbol* SymTbls::lookup(strptr name) {
  // Search the active symbol tables for the symbol whose name is pointed
  // to by 'name'.  Return a pointer to the symbol if found, NULL otherwise.

  for (SymTbl* tp = activeList; tp; tp = tp->next) {
    Symbol* sp = tp->lookup(name);
    if (sp) return sp;
  }

  return 0;
}

void SymTbls::delFreeTbls() {
  // Delete all symbol tables which do not have their 'keep' flag set.

  SymTbl* sp;
  SymTbl* ptr;

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

bool SymTbls::del(strptr name) {
  // Search the active symbol tables for the symbol whose name
  // is pointed to by 'name'.  If it is found, delete the symbol
  // and return True, else return False.

  for (SymTbl* tp = activeList; tp; tp = tp->next)
    if (tp->del(name)) return True;

  return False;
}

Symbol* SymTbls::remove(strptr name) {
  // Search the active symbol tables for the symbol whose name is pointed
  // to by 'name'.  Remove it and return a pointer to the symbol if found,
  // return 0 otherwise.

  for (SymTbl* tp = activeList; tp; tp = tp->next) {
    Symbol* sp = tp->remove(name);
    if (sp) return sp;
  }

  return 0;
}

void SymTbls::deactivate(SymTbl* tbl) {
  // Add this symbol table to the inactive list.

  // Remove the symbol table from the symbol table list.
  unlink(tbl);

  if (!listCode)
    delete tbl;

  else {
    // See if the table is already in the inactive list.
    SymTbl* tp;
    for (tp = inactiveList; tp && tp != tbl; tp = tp->next);

    // If the table is not in the inactive list, add it.
    if (!tp) {
      tbl->next = inactiveList;
      inactiveList = tbl;
    }
  }
}

void SymTbls::unlink(SymTbl* tbl) {
  // Unlink this symbol table from the active list.

  if (tbl == activeList)
    activeList = tbl->next;

  else {
    // Search for the symbol table preceeding this one in the list.
    SymTbl* tp;
    for (tp = activeList; tp && tp->next != tbl; tp = tp->next);

    // Link around this table.
    if (tp) tp->next = tbl->next;
  }
}
