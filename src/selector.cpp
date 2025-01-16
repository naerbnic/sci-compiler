//	selector.cpp
// 	code for handling selectors

#include <stdio.h>
#include <string.h>

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

  for (Selector* sn = super->selectors; sn; sn = sn->next) {
    Selector* tn = new Selector;
    *tn = *sn;
    if (tn->tag == T_LOCAL) tn->tag = T_METHOD;  // No longer a local method.

    if (!selectors)
      selectors = selTail = tn;
    else {
      selTail->next = tn;
      selTail = tn;
    }
  }

  numProps = super->numProps;
}

Selector* Object::findSelector(Symbol* sym) {
  // Return a pointer to the selector node which corresponds to the
  //	symbol 'sym'.

  int val = sym->val();
  Selector* sn;
  for (sn = selectors; sn && val != sn->sym->val(); sn = sn->next);

  return sn;
}

Selector* Object::findSelector(strptr name) {
  // Return a pointer to the selector node which has the name 'name'.

  Symbol* sym = syms.lookup(name);
  return sym ? findSelector(sym) : 0;
}

void Object::freeSelectors() {
  // free the object's selectors

  Selector* next;
  for (Selector* s = selectors; s; s = next) {
    next = s->next;
    delete s;
  }
  selectors = 0;
  selTail = 0;
}

///////////////////////////////////////////////////////////////////////////

Selector* Class::addSelector(Symbol* sym, int what) {
  // Add a selector ('sym') to the selectors for this class.
  // Allocate a selector node, and link it into the selector list for
  // the class.  Finally, return a pointer to the selector node.

  if (!sym) return 0;

  Selector* sn = new Selector(sym);

  if (!selectors)
    // This is the first selector in the class.
    selectors = selTail = sn;
  else {
    // Link the selector in at the end of those for this class.
    selTail->next = sn;
    selTail = sn;
  }

  switch (sym->val()) {
    case SEL_METHDICT:
      sn->tag = T_METHDICT;
      break;
    case SEL_PROPDICT:
      sn->tag = T_PROPDICT;
      break;
    default:
      sn->tag = (ubyte)what;
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

  char selStr[200];

  for (Symbol* sym = LookupTok(); !CloseP(symType); sym = LookupTok()) {
    // Make sure that the symbol is not already defined.
    if (sym && symType != S_SELECT) {
      Error("Redefinition of %s.", symStr);
      GetToken();  // eat selector number
      if (!IsNumber()) UnGetTok();
      continue;
    }
    strcpy(selStr, symStr);

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

Symbol* InstallSelector(strptr name, int value) {
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
  if (symType != S_IDENT) {
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
  tokSym = *msgSel;

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
    switch ((uint32_t)obj->val()) {
      case OBJ_SELF:
        receiver = curObj;
        break;
      case OBJ_SUPER:
        /* Dont try to find super of RootObj */
        if (curObj->super >= 0)
          receiver = classes[curObj->super];
        else {
          receiver = curObj;
          Severe("RootObj has no super.");
        }
        break;
      default:
        receiver = obj->obj();
        break;
    }
    if (!receiver->findSelector(&tokSym)) {
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
