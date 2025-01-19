//	symtbl.cpp	sc
// 	symbol table routines for sc

#include "symtbl.hpp"

#include <string.h>

#include <memory>

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

  for (auto const& [dummy, sym] : symbols) sym->clearAn();
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

  auto sp = std::unique_ptr<SymTbl>(new SymTbl(size, keep));
  auto* sp_ptr = sp.get();
  activeList.push_front(std::move(sp));
  return sp_ptr;
}

void SymTbls::clearAsmPtrs() {
  selectorSymTbl->clearAsmPtrs();
  classSymTbl->clearAsmPtrs();
  globalSymTbl->clearAsmPtrs();
}

Symbol* SymTbls::lookup(strptr name) {
  // Search the active symbol tables for the symbol whose name is pointed
  // to by 'name'.  Return a pointer to the symbol if found, NULL otherwise.

  for (auto const& tp : activeList) {
    Symbol* sp = tp->lookup(name);
    if (sp) return sp;
  }

  return nullptr;
}

void SymTbls::delFreeTbls() {
  // Delete all symbol tables which do not have their 'keep' flag set.
  auto new_end = std::remove_if(activeList.begin(), activeList.end(),
                                [](auto& sp) { return !sp->keep; });
  activeList.erase(new_end, activeList.end());
  inactiveList.clear();
}

bool SymTbls::del(strptr name) {
  // Search the active symbol tables for the symbol whose name
  // is pointed to by 'name'.  If it is found, delete the symbol
  // and return True, else return False.

  for (auto const& tp : activeList) {
    if (tp->del(name)) return true;
  }

  return false;
}

Symbol* SymTbls::remove(strptr name) {
  // Search the active symbol tables for the symbol whose name is pointed
  // to by 'name'.  Remove it and return a pointer to the symbol if found,
  // return 0 otherwise.

  for (auto& tp : activeList) {
    Symbol* sp = tp->remove(name);
    if (sp) return sp;
  }

  return 0;
}

void SymTbls::deactivate(SymTbl* tbl) {
  // Add this symbol table to the inactive list.

  auto it = std::find_if(activeList.begin(), activeList.end(),
                         [tbl](auto& sp) { return sp.get() == tbl; });
  if (it == activeList.end()) return;

  std::unique_ptr<SymTbl> owned_tbl = std::move(*it);
  activeList.erase(it);

  if (listCode) {
    inactiveList.push_front(std::move(owned_tbl));
  }
}
