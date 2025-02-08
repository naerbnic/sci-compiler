//	proc.cpp		sc

#include "scic/proc.hpp"

#include <memory>

#include "scic/compile.hpp"
#include "scic/error.hpp"
#include "scic/expr.hpp"
#include "scic/object.hpp"
#include "scic/parse.hpp"
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
static void NewParm(int n, sym_t type);
static void AddRest(int ofs);

void Procedure() {
  // 	procedure ::= 'procedure' call-def [expression+]
  //	OR
  //		procedure ::= procedure procedure-name+

  Symbol* theSym;
  SymTbl* theSymTbl;

  GetToken();
  UnGetTok();
  if (symType() == OPEN_P) {
    // Then a procedure definition.
    theSymTbl = gSyms.add(ST_MINI);

    {
      auto theNode = CallDef(S_PROC);
      if (theNode) {
        ExprList(theNode.get(), OPTIONAL);
        CompileProc(gSc->hunkList->getList(), theNode.get());
      }
    }

    gSyms.deactivate(theSymTbl);

  } else {
    // A procedure declaration.
    for (GetToken(); !CloseP(symType()); GetToken()) {
      if (symType() == S_IDENT) theSym = gSyms.installLocal(gSymStr, S_PROC);
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

  GetToken();
  theProc = gSyms.lookup(gSymStr);
  switch (theType) {
    case S_PROC:
      if (!theProc)
        theProc = gSyms.installModule(gSymStr, theType);

      else if (theProc->type != S_PROC || theProc->val() != UNDEFINED) {
        Severe("%s is already defined.", gSymStr);
        return 0;
      }

      theProc->setVal(DEFINED);
      break;

    case S_SELECT:
      if (!theProc || !(sn = gCurObj->findSelectorByNum(theProc->val())) ||
          IsProperty(sn)) {
        Severe("%s is not a method for class %s", gSymStr,
               gCurObj->sym->name());
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
  for (LookupTok(); !CloseP(symType()); LookupTok()) {
    if (symType() == S_KEYWORD && symVal() == K_TMP) {
      // Now defining temporaries -- set 'rest of argument' value.
      AddRest(parmOfs);
      parmOfs = 0;
      parmType = S_TMP;

    } else if (symType() == S_IDENT)
      // A parameter or tmp variable definition.
      NewParm(parmOfs++, parmType);

    else if (symType() == S_OPEN_BRACKET) {
      // An array parameter or tmp variable.
      if (!GetIdent()) break;
      NewParm(parmOfs, parmType);
      if (!GetNumber("array size")) return 0;
      parmOfs += symVal();
      GetToken();
      if (symType() != (sym_t)']') {
        Error("expecting closing ']': %s.", gSymStr);
        UnGetTok();
      }

    } else if (symType() == S_SELECT) {
      if (gCurObj && gCurObj->findSelectorByNum(gTokSym.val()))
        Error("%s is a selector for current object.", gSymStr);
      else
        gSyms.installLocal(gSymStr, parmType)->setVal(parmOfs++);

    } else
      Error("Non-identifier in parameter list: %s", gSymStr);
  }

  if (parmType == S_PARM) AddRest(parmOfs);

  gInParmList = false;

  UnGetTok();

  // Return the number of temporary variables.
  return parmType == S_PARM ? 0 : parmOfs;
}

static void NewParm(int n, sym_t type) {
  Symbol* theSym;

  if (gSyms.lookup(gSymStr)) Warning("Redefinition of '%s'.", gSymStr);
  theSym = gSyms.installLocal(gSymStr, type);
  theSym->setVal(n);
}

static void AddRest(int ofs) {
  Symbol* theSym;

  theSym = gSyms.installLocal("&rest", S_REST);
  theSym->setVal(ofs);
}
