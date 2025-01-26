// symtbl.hpp
//		definitions for symbol tables

#ifndef SYMTBL_HPP
#define SYMTBL_HPP

#include <memory>
#include <ostream>

#include "absl/container/btree_map.h"
#include "symbol.hpp"

// Possible hash table sizes for symbol tables.
const int ST_LARGE = 253;
const int ST_MEDIUM = 89;
const int ST_SMALL = 19;
const int ST_MINI = 5;

class SymTbl {
  // The SymTbl class is a collection of Symbols.  It is implemented as a hash
  // table of user-selectable size (#defines for some sizes given above).
  // A symbol to be added to the table has its name hashed by the hash() method,
  // and the hash value is used to index into the hash table, which holds a
  // pointer to the first element of a list of symbols which hash to that value.
  // To keep the lists short, the table size should be larger for tables
  // expected to hold more symbols.
  //
  // SymTbls are created globally, for each module, and for each method or
  // procedure compiled.  They are linked into FIFO list called 'activeList' in
  // order to implement block-structured scoping.  When the symbols in a table
  // go out of scope (when we're done compiling a method, for example), they are
  // moved to 'inactiveList' if they are needed to generate a listing later
  // or are deleted if no listing has been requested.

  using SymbolMap = absl::btree_map<std::string, std::unique_ptr<Symbol>>;

 public:
  ~SymTbl();

  using iterator = SymbolMap::iterator;

  Symbol* add(std::unique_ptr<Symbol> sym);
  // Add the Symbol 'sym' to this table.

  Symbol* lookup(std::string_view name);
  // Return a pointer to the symbol with name 'name' if it is in
  // this table, 0 otherwise.

  iterator begin() { return symbols.begin(); }
  iterator end() { return symbols.end(); }

 private:
  SymTbl(int size = ST_MEDIUM, bool retain = false);

  Symbol* install(std::string_view name, sym_t type);
  // Install the identifier 'name' as a Symbol of type 'type'.
  // Return a pointer to it.

  std::unique_ptr<Symbol> remove(strptr name);
  // Remove the Symbol with name 'name' from this table.  Return
  // a pointer to the Symbol.

  bool del(strptr name);
  // Remove and delete a Symbol with name 'name' from the table.
  // Return True if it was found and deleted, False otherwise.

  void retain(bool keepIt = true) { keep = keepIt; }
  // Set the 'keep' property to the value of 'keepIt'

  void clearAsmPtrs();
  // Make sure that all pointers in symbols in the symbol table
  //	refering to ANodes is cleared.

  SymbolMap symbols;
  bool keep;  // should this table be kept for listings when out of scope?

  friend class SymTbls;

  friend std::ostream& operator<<(std::ostream& os, const SymTbl& symtbl);
};

class SymTbls {
 public:
  SymTbls();

  SymTbl* add(int size = ST_MEDIUM, bool retain = false);
  //	add a New symbol table and link it into the list

  void clearAsmPtrs();

  void deactivate(SymTbl*);
  // Remove this table from the active SymTbl list ('activeList').
  // Delete it if not necessary for the listing, add it to
  // 'inactiveList' otherwise.

  bool del(strptr name);
  // Delete the symbol with name 'name' from the SymTbls in activeList
  // Return True if the symbol was present, False otherwise.

  Symbol* installLocal(strptr n, sym_t t) {
    return activeList.front()->install(n, t);
  }
  Symbol* installModule(strptr n, sym_t t) {
    return moduleSymTbl->install(n, t);
  }
  Symbol* installGlobal(std::string_view n, sym_t t) {
    return globalSymTbl->install(n, t);
  }
  Symbol* installClass(strptr n) { return classSymTbl->install(n, S_CLASS); }
  Symbol* installSelector(strptr n) {
    return selectorSymTbl->install(n, S_SELECT);
  }

  void delFreeTbls();
  // Delete all symbol tables which do not have their 'keep' property
  //	set

  Symbol* lookup(std::string_view name);
  // syms.lookup the Symbol with name 'name' in 'activeList'. Return 0
  // if not found.

  std::unique_ptr<Symbol> remove(strptr name);
  // Remove the symbol with name 'name' from the SymTbls in activeList
  // Return a pointer to the symbol if found, NULL otherwise.

  SymTbl* classSymTbl;
  SymTbl* selectorSymTbl;
  SymTbl* moduleSymTbl;

 private:
  std::deque<std::unique_ptr<SymTbl>> activeList;
  std::deque<std::unique_ptr<SymTbl>> inactiveList;

  SymTbl* globalSymTbl;

  friend std::ostream& operator<<(std::ostream& os, const SymTbls& symtbl);
};

extern SymTbls syms;

#endif
