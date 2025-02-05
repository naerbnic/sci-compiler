//	toktypes.cpp	sc
// 	routines for restricting the types of tokens returned or checking on
// 	symbol/token types

#include "scic/toktypes.hpp"

#include "scic/error.hpp"
#include "scic/expr.hpp"
#include "scic/object.hpp"
#include "scic/proc.hpp"
#include "scic/sc.hpp"
#include "scic/symtbl.hpp"
#include "scic/token.hpp"

int gSelectorIsVar;

static Symbol* Immediate();
static bool GetNumberOrStringToken(std::string_view errStr, bool stringOK);

Symbol* LookupTok() {
  // Get a token.  If it is an identifier, look it up in the current
  // environment and put its values in the global token slot.  Return a
  // a pointer to the symbol in the table.

  Symbol* theSym;

  GetToken();

  if (symType() == (sym_t)'#') return Immediate();

  if (symType() == S_IDENT && (theSym = gSyms.lookup(gSymStr))) {
    gTokSym.SaveSymbol(*theSym);
    gTokSym.clearName();
  } else
    theSym = 0;

  if (symType() == S_SELECT) {
    if (gCurObj && !gCurObj->selectors().empty()) {
      // If the symbol is a property and we're in a method definition,
      //	access the symbol as a local variable.

      // A selector list is in effect -- check that the selector
      //	reference is legal (i.e. it is a property in the current
      //	selector list).
      auto* sn = gCurObj->findSelectorByNum(theSym->val());
      if (!sn) {
        if (!gInParmList) {
          Error("Not a selector for current class/object: %s", theSym->name());
          theSym = 0;
        }

      } else if (sn->tag != T_LOCAL && sn->tag != T_METHOD) {
        setSymType(S_PROP);
        setSymVal(sn->ofs);
      }
    }
  }

  return theSym;
}

Symbol* GetSymbol() {
  // Get a token that is in the symbol table.
  Symbol* theSym;
  GetToken();
  if (!(theSym = gSyms.lookup(gSymStr))) {
    Severe("%s not defined.", gSymStr);
    return nullptr;
  }

  return theSym;
}

bool GetIdent() {
  // Get an identifier.

  GetToken();
  return IsUndefinedIdent();
}

bool GetDefineSymbol() {
  //	gets a symbol that was previously defined

  NextToken();
  if (symType() != S_IDENT) {
    Error("Defined symbol expected");
    return false;
  }
  Symbol* sym = gSyms.lookup(gSymStr);
  if (!sym) return false;
  if (sym->type != S_DEFINE) {
    Error("Define expected");
    return false;
  }
  return true;
}

bool IsIdent() {
  if (symType() != S_IDENT) {
    Severe("Identifier required: %s", gSymStr);
    return false;
  }

  return true;
}

bool IsUndefinedIdent() {
  if (!IsIdent()) return false;

  if (gSyms.lookup(gSymStr)) Warning("Redefinition of %s.", gSymStr);

  return true;
}

bool GetNumber(std::string_view errStr) {
  return GetNumberOrStringToken(errStr, false);
}

bool GetNumberOrString(std::string_view errStr) {
  return GetNumberOrStringToken(errStr, true);
}

static bool GetNumberOrStringToken(std::string_view errStr, bool stringOK) {
  // Get a number (or a string) from the input.

  // Get a parse node.
  auto pn = std::make_unique<PNode>(PN_EXPR);

  // Get an expression.
  Expression(pn.get(), REQUIRED);

  // If the expression is not a constant, bitch.
  pn_t type = pn->first_child()->type;
  if (type != PN_NUM && (type != PN_STRING || !stringOK)) {
    Error("%s required.", errStr);
    return false;
  }

  // Otherwise, put the expression value into the symbol variables.
  switch (type) {
    case PN_NUM:
      setSymType(S_NUM);
      break;
    case PN_STRING:
      setSymType(S_STRING);
      break;
    default:
      Fatal("Unexpected literal type");
      break;
  }

  setSymVal(pn->first_child()->val);

  return true;
}

bool GetString(std::string_view errStr) {
  // Get a string from the input.

  GetToken();
  if (symType() != S_STRING) {
    Severe("%s required: %s", errStr, gSymStr);
    return false;
  }

  return true;
}

keyword_t Keyword() {
  Symbol* theSym;

  if (!(theSym = gSyms.lookup(gSymStr)) || theSym->type != S_KEYWORD)
    return K_UNDEFINED;
  else {
    setSymType(S_KEYWORD);
    setSymVal(theSym->val());
    return (keyword_t)symVal();
  }
}

void GetKeyword(keyword_t which) {
  std::string_view str;

  GetToken();
  if (Keyword() != which) {
    switch (which) {
      case K_OF:
        str = "of";
        break;

      case K_SCRIPTNUM:
        str = "script#";
        break;

      case K_CLASSNUM:
        str = "class#";
        break;

      default:
        Fatal("Internal error: GetKeyword.");
    }

    Error("%s keyword missing.", str);
    UnGetTok();
  }
}

bool IsVar() {
  // return whether the current symbol is a variable

  Selector* sn;

  switch (symType()) {
    case S_GLOBAL:
    case S_LOCAL:
    case S_TMP:
    case S_PARM:
    case S_PROP:
    case S_OPEN_BRACKET:
      return true;

    case S_SELECT:
      return gCurObj && gSelectorIsVar &&
             (sn = gCurObj->findSelectorByNum(gTokSym.val())) &&
             sn->tag == T_PROP;

    default:
      return false;
  }
}

bool IsProc() {
  // If the current symbol is a procedure of some type, return the type.
  // Otherwise return false.

  return symType() == S_PROC || symType() == S_EXTERN;
}

bool IsObj() {
  return symType() == S_OBJ || symType() == S_CLASS || symType() == S_IDENT ||
         symType() == OPEN_P || IsVar();
}

bool IsNumber() { return symType() == S_NUM || symType() == S_STRING; }

static Symbol* Immediate() {
  Symbol* theSym = nullptr;

  GetToken();
  if (symType() == S_IDENT) {
    if (!(theSym = gSyms.lookup(gSymStr)) || theSym->type != S_SELECT) {
      Error("Selector required: %s", gSymStr);
      return 0;
    }
    gTokSym.SaveSymbol(*theSym);
    gTokSym.type = S_NUM;
  }
  return theSym;
}
