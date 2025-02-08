//	selector.cpp
// 	code for handling selectors

#include "scic/selector.hpp"

#include <cstdint>
#include <string>
#include <string_view>

#include "scic/class.hpp"
#include "scic/config.hpp"
#include "scic/error.hpp"
#include "scic/object.hpp"
#include "scic/symbol.hpp"
#include "scic/symtbl.hpp"
#include "scic/symtypes.hpp"
#include "scic/token.hpp"
#include "scic/toktypes.hpp"
#include "scic/update.hpp"

int gMaxSelector;

const int MAXSELECTOR = 8192;
const int BITS_PER_ENTRY = 16;
const int SEL_TBL_SIZE = MAXSELECTOR / BITS_PER_ENTRY;

static void ClaimSelectorNum(uint16_t n);
static uint16_t selTbl[SEL_TBL_SIZE];

///////////////////////////////////////////////////////////////////////////

void InitSelectors() {
  // Add the selectors to the selector symbol table.

  for (ResolvedTokenSlot slot = LookupTok(); !CloseP(slot.type());
       slot = LookupTok()) {
    // Make sure that the symbol is not already defined.
    if (slot.is_resolved() && slot.type() != S_SELECT) {
      Error("Redefinition of %s.", slot.name());
      auto num_token = GetToken();  // eat selector number
      if (!IsNumber(num_token)) UnGetTok();
      continue;
    }

    std::string selStr(slot.name());

    int sel_num = GetNumber("Selector number").value_or(0);
    if (!slot.is_resolved())
      InstallSelector(selStr, sel_num);
    else
      slot.symbol()->setVal(sel_num);
  }

  UnGetTok();

  // The selectors just added were read from the file 'selector'.  Thus
  // there is no reason to rewrite that file.
  gSelectorAdded = false;
}

Symbol* InstallSelector(std::string_view name, int value) {
  // Add 'name' to the global symbol table as a selector with value 'value'.

  // Allocate this selector number.
  ClaimSelectorNum(value);

  // Since this is a new selector, we'll need to rewrite the file 'selector'.
  gSelectorAdded = true;

  // Install the selector in the selector symbol table.
  Symbol* sym = gSyms.installSelector(name);
  sym->setVal(value);

  return sym;
}

int NewSelectorNum() {
  // Allocate a new selector number and return it.  selTbl is an array of bits,
  // corresponding to the selector numbers.  If a bit is set, its corresponding
  // number is allocated.

  // Scan for the first entry with a free bit.
  uint16_t* stp;
  for (stp = selTbl; stp < &selTbl[SEL_TBL_SIZE] && *stp == (uint16_t)-1; ++stp)
    ;

  // Check for no more selector numbers available.
  if (stp >= &selTbl[SEL_TBL_SIZE]) Fatal("Out of selector numbers!");

  // Find the specific selector number that is free.
  uint16_t n = uint16_t((stp - &selTbl[0]) * BITS_PER_ENTRY);
  for (uint16_t mask = 0x8000; mask & *stp; mask >>= 1, ++n)
    ;

  return n;
}

Symbol* GetSelector(Symbol* obj) {
  Symbol* msgSel;

  // Get the next token.  If it's not an identifier, it can't be a selector.
  auto token = GetToken();
  if (token.type() == (sym_t)',') token = GetToken();
  if (token.type() != S_SELECT_LIT) {
    UnGetTok();
    return 0;
  }

  // Look up the identifier.  If it is not currently defined, define it as
  // the next selector number.
  if (!(msgSel = gSyms.lookup(token.name()))) {
    InstallSelector(token.name(), NewSelectorNum());
    msgSel = gSyms.lookup(token.name());
    if (gConfig->showSelectors)
      Info("%s is being installed as a selector.", token.name());
  }
  auto slot = ResolvedTokenSlot::OfSymbol(msgSel);

  // The symbol must be either a variable or a selector.
  if (slot.type() != S_SELECT && !IsVar(slot)) {
    Severe("Selector required: %s", slot.name());
    return 0;
  }

  // Complain if the symbol is a variable, but a selector of the same name
  //	exists.
  if (IsVar(slot) && slot.type() != S_PROP && slot.type() != S_SELECT &&
      gSyms.selectorSymTbl->lookup(slot.name())) {
    Error("%s is both a selector and a variable.", slot.name());
    return 0;
  }

  // The selector must be a selector for the object 'obj', if that
  // object is known.
  gReceiver = 0;
  if (!IsVar(slot) && obj && (obj->type == S_OBJ || obj->type == S_CLASS) &&
      obj->obj()) {
    if (obj->hasVal(OBJ_SELF)) {
      gReceiver = gCurObj;
    } else if (obj->hasVal(OBJ_SUPER)) {
      /* Dont try to find super of RootObj */
      if (gCurObj->super >= 0)
        gReceiver = gClasses[gCurObj->super];
      else {
        gReceiver = gCurObj;
        Severe("RootObj has no super.");
      }
    } else {
      gReceiver = obj->obj();
    }

    if (!gReceiver->findSelectorByNum(slot.val())) {
      Error("Not a selector for %s: %s", obj->name(), slot.name());
      return 0;
    }
  }

  return msgSel;
}

static void ClaimSelectorNum(uint16_t n) {
  // Claim selector number n.

  if (n > MAXSELECTOR) Fatal("Attempt to claim illegal selector!");

  selTbl[n / BITS_PER_ENTRY] |= uint16_t(0x8000 >> (n % BITS_PER_ENTRY));

  if (n > gMaxSelector) gMaxSelector = n;
}
