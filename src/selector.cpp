//	selector.cpp
// 	code for handling selectors

#include <stdio.h>

#include "error.hpp"
#include "object.hpp"
#include "sc.hpp"
#include "sol.hpp"
#include "symtbl.hpp"
#include "token.hpp"
#include "update.hpp"

int maxSelector;
bool showSelectors;

const int MAXSELECTOR = 8192;
const int BITS_PER_ENTRY = 16;
const int SEL_TBL_SIZE = MAXSELECTOR / BITS_PER_ENTRY;

static void ClaimSelectorNum(uint16_t n);
static uint16_t selTbl[SEL_TBL_SIZE];

void Object::dupSelectors(Class* super) {
  // duplicate super's selectors

  for (auto* sn : super->selectors()) {
    auto tn = std::make_unique<Selector>();
    *tn = *sn;
    if (tn->tag == T_LOCAL) {
      tn->tag = T_METHOD;  // No longer a local method.
      tn->an = nullptr;    // No code defined for this class.
    }
    selectors_.push_back(std::move(tn));
  }

  numProps = super->numProps;
}

Selector* Object::findSelectorByNum(int val) {
  // Return a pointer to the selector node which corresponds to the
  //	symbol 'sym'.

  for (auto* sn : selectors()) {
    if (val == sn->sym->val()) return sn;
  }

  return nullptr;
}

Selector* Object::findSelector(std::string_view name) {
  // Return a pointer to the selector node which has the name 'name'.

  Symbol* sym = syms.lookup(name);
  return sym ? findSelectorByNum(sym->val()) : 0;
}

void Object::freeSelectors() {
  // free the object's selectors

  selectors_.clear();
}

///////////////////////////////////////////////////////////////////////////

Selector* Class::addSelector(Symbol* sym, int what) {
  // Add a selector ('sym') to the selectors for this class.
  // Allocate a selector node, and link it into the selector list for
  // the class.  Finally, return a pointer to the selector node.

  if (!sym) return 0;

  auto sn_owned = std::make_unique<Selector>(sym);
  auto* sn = sn_owned.get();
  selectors_.push_back(std::move(sn_owned));

  switch (sym->val()) {
    case SEL_METHDICT:
      sn->tag = T_METHDICT;
      break;
    case SEL_PROPDICT:
      sn->tag = T_PROPDICT;
      break;
    default:
      sn->tag = (uint8_t)what;
      break;
  }

  if (PropTag(what)) {
    sn->ofs = 2 * numProps;
    ++numProps;
  }

  return sn;
}

///////////////////////////////////////////////////////////////////////////

void InitSelectors() {
  // Add the selectors to the selector symbol table.

  for (Symbol* sym = LookupTok(); !CloseP(symType); sym = LookupTok()) {
    // Make sure that the symbol is not already defined.
    if (sym && symType != S_SELECT) {
      Error("Redefinition of %s.", symStr);
      GetToken();  // eat selector number
      if (!IsNumber()) UnGetTok();
      continue;
    }

    std::string selStr = symStr;

    GetNumber("Selector number");
    if (!sym)
      InstallSelector(selStr, symVal);
    else
      sym->setVal(symVal);
  }

  UnGetTok();

  // The selectors just added were read from the file 'selector'.  Thus
  // there is no reason to rewrite that file.
  selectorAdded = False;
}

Symbol* InstallSelector(std::string_view name, int value) {
  // Add 'name' to the global symbol table as a selector with value 'value'.

  // Allocate this selector number.
  ClaimSelectorNum(value);

  // Since this is a new selector, we'll need to rewrite the file 'selector'.
  selectorAdded = True;

  // Install the selector in the selector symbol table.
  Symbol* sym = syms.installSelector(name);
  sym->setVal(value);

  return sym;
}

int NewSelectorNum() {
  // Allocate a new selector number and return it.  selTbl is an array of bits,
  // corresponding to the selector numbers.  If a bit is set, its corresponding
  // number is allocated.

  // Scan for the first entry with a free bit.
  uint16_t* stp;
  for (stp = selTbl; stp < &selTbl[SEL_TBL_SIZE] && *stp == (uint16_t)-1;
       ++stp);

  // Check for no more selector numbers available.
  if (stp >= &selTbl[SEL_TBL_SIZE]) Fatal("Out of selector numbers!");

  // Find the specific selector number that is free.
  uint16_t n = uint16_t((stp - &selTbl[0]) * BITS_PER_ENTRY);
  for (uint16_t mask = 0x8000; mask & *stp; mask >>= 1, ++n);

  return n;
}

Symbol* GetSelector(Symbol* obj) {
  Symbol* msgSel;

  // Get the next token.  If it's not an identifier, it can't be a selector.
  GetToken();
  if (symType == (sym_t)',') GetToken();
  if (symType != S_SELECT_LIT) {
    UnGetTok();
    return 0;
  }

  // Look up the identifier.  If it is not currently defined, define it as
  // the next selector number.
  if (!(msgSel = syms.lookup(symStr))) {
    InstallSelector(symStr, NewSelectorNum());
    msgSel = syms.lookup(symStr);
    if (showSelectors) Info("%s is being installed as a selector.", symStr);
  }
  tokSym.SaveSymbol(*msgSel);

  // The symbol must be either a variable or a selector.
  if (symType != S_SELECT && !IsVar()) {
    Severe("Selector required: %s", symStr);
    return 0;
  }

  // Complain if the symbol is a variable, but a selector of the same name
  //	exists.
  if (IsVar() && symType != S_PROP && symType != S_SELECT &&
      syms.selectorSymTbl->lookup(symStr)) {
    Error("%s is both a selector and a variable.", symStr);
    return 0;
  }

  // The selector must be a selector for the object 'obj', if that
  // object is known.
  receiver = 0;
  if (!IsVar() && obj && (obj->type == S_OBJ || obj->type == S_CLASS) &&
      obj->obj()) {
    if (obj->hasVal(OBJ_SELF)) {
      receiver = curObj;
    } else if (obj->hasVal(OBJ_SUPER)) {
      /* Dont try to find super of RootObj */
      if (curObj->super >= 0)
        receiver = classes[curObj->super];
      else {
        receiver = curObj;
        Severe("RootObj has no super.");
      }
    } else {
      receiver = obj->obj();
    }

    if (!receiver->findSelectorByNum(tokSym.val())) {
      Error("Not a selector for %s: %s", obj->name(), tokSym.name());
      return 0;
    }
  }

  return msgSel;
}

static void ClaimSelectorNum(uint16_t n) {
  // Claim selector number n.

  if (n > MAXSELECTOR) Fatal("Attempt to claim illegal selector!");

  selTbl[n / BITS_PER_ENTRY] |= uint16_t(0x8000 >> (n % BITS_PER_ENTRY));

  if (n > maxSelector) maxSelector = n;
}
