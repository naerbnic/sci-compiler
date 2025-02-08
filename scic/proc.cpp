//	proc.cpp		sc

#include "scic/proc.hpp"

#include <memory>

#include "scic/compile.hpp"
#include "scic/error.hpp"
#include "scic/expr.hpp"
#include "scic/object.hpp"
#include "scic/parse.hpp"
#include "scic/parse_context.hpp"
#include "scic/pnode.hpp"
#include "scic/sc.hpp"
#include "scic/selector.hpp"
#include "scic/symbol.hpp"
#include "scic/symtbl.hpp"
#include "scic/symtypes.hpp"
#include "scic/token.hpp"
#include "scic/toktypes.hpp"

bool gInParmList;

static std::unique_ptr<PNode> _CallDef(sym_t theType);
static int ParameterList();
static void NewParm(int n, sym_t type, TokenSlot const& token);
static void AddRest(int ofs);

void Procedure() {
  // 	procedure ::= 'procedure' call-def [expression+]
  //	OR
  //		procedure ::= procedure procedure-name+

  Symbol* theSym;
  SymTbl* theSymTbl;

  auto token = GetToken();
  UnGetTok();
  if (token.type() == OPEN_P) {
    // Then a procedure definition.
    theSymTbl = gParseContext.syms.add(ST_MINI);

    {
      auto theNode = CallDef(S_PROC);
      if (theNode) {
        ExprList(theNode.get(), OPTIONAL);
        CompileProc(gSc->hunkList->getList(), theNode.get());
      }
    }

    gParseContext.syms.deactivate(theSymTbl);

  } else {
    // A procedure declaration.
    for (token = GetToken(); !CloseP(token.type()); token = GetToken()) {
      if (token.type() == S_IDENT)
        theSym = gParseContext.syms.installLocal(token.name(), S_PROC);
      theSym->setVal(UNDEFINED);
    }
    UnGetTok();
  }
}

std::unique_ptr<PNode> CallDef(sym_t theType) {
  // call-def ::= open _call-def close

  if (!OpenBlock()) {
    UnGetTok();
    Error("expected opening parenthesis or brace.");
    return 0;
  }

  auto theNode = _CallDef(theType);
  CloseBlock();

  return theNode;
}

static std::unique_ptr<PNode> _CallDef(sym_t theType) {
  // _call-def ::= symbol [variable+] [&tmp variable+]

  Symbol* theProc;
  Selector* sn;

  auto token = GetToken();
  theProc = gParseContext.syms.lookup(token.name());
  switch (theType) {
    case S_PROC:
      if (!theProc)
        theProc = gParseContext.syms.installModule(token.name(), theType);

      else if (theProc->type != S_PROC || theProc->val() != UNDEFINED) {
        Severe("%s is already defined.", token.name());
        return 0;
      }

      theProc->setVal(DEFINED);
      break;

    case S_SELECT:
      if (!theProc || !(sn = gParseContext.curObj->findSelectorByNum(theProc->val())) ||
          IsProperty(sn)) {
        Severe("%s is not a method for class %s", token.name(), gParseContext.curObj->name);
        return 0;
      }
      break;

    default:
      Fatal("Invalid symbol type in _CallDef: %d", theType);
      break;
  }

  auto theNode =
      std::make_unique<PNode>(theType == S_SELECT ? PN_METHOD : PN_PROC);
  theNode->sym = theProc;
  theNode->val = ParameterList();  // number of temporary variables

  return theNode;
}

static int ParameterList() {
  // parameter-list ::= [variable+] [&tmp variable+]

  int parmOfs;
  sym_t parmType;

  parmOfs = 1;
  parmType = S_PARM;

  gInParmList = true;
  for (auto slot = LookupTok(); !CloseP(slot.type()); slot = LookupTok()) {
    if (slot.type() == S_KEYWORD && slot.val() == K_TMP) {
      // Now defining temporaries -- set 'rest of argument' value.
      AddRest(parmOfs);
      parmOfs = 0;
      parmType = S_TMP;

    } else if (slot.type() == S_IDENT)
      // A parameter or tmp variable definition.
      NewParm(parmOfs++, parmType, slot.token());

    else if (slot.type() == S_OPEN_BRACKET) {
      // An array parameter or tmp variable.
      auto name = GetIdent();
      if (!name) break;
      NewParm(parmOfs, parmType, *name);
      auto array_size = GetNumber("array size");
      if (!array_size) return 0;
      parmOfs += *array_size;
      auto close = GetToken();
      if (close.type() != (sym_t)']') {
        Error("expecting closing ']': %s.", close.name());
        UnGetTok();
      }

    } else if (slot.type() == S_SELECT) {
      if (gParseContext.curObj && gParseContext.curObj->findSelectorByNum(slot.val()))
        Error("%s is a selector for current object.", slot.name());
      else
        gParseContext.syms.installLocal(slot.name(), parmType)->setVal(parmOfs++);

    } else
      Error("Non-identifier in parameter list: %s", slot.name());
  }

  if (parmType == S_PARM) AddRest(parmOfs);

  gInParmList = false;

  UnGetTok();

  // Return the number of temporary variables.
  return parmType == S_PARM ? 0 : parmOfs;
}

static void NewParm(int n, sym_t type, TokenSlot const& token) {
  Symbol* theSym;

  if (gParseContext.syms.lookup(token.name()))
    Warning("Redefinition of '%s'.", token.name());
  theSym = gParseContext.syms.installLocal(token.name(), type);
  theSym->setVal(n);
}

static void AddRest(int ofs) {
  Symbol* theSym;

  theSym = gParseContext.syms.installLocal("&rest", S_REST);
  theSym->setVal(ofs);
}
