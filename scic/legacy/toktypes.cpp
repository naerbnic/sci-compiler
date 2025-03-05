//	toktypes.cpp	sc
// 	routines for restricting the types of tokens returned or checking on
// 	symbol/token types

#include "scic/legacy/toktypes.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/strings/str_cat.h"
#include "scic/legacy/common.hpp"
#include "scic/legacy/error.hpp"
#include "scic/legacy/expr.hpp"
#include "scic/legacy/object.hpp"
#include "scic/legacy/parse_context.hpp"
#include "scic/legacy/pnode.hpp"
#include "scic/legacy/proc.hpp"
#include "scic/legacy/selector.hpp"
#include "scic/legacy/symbol.hpp"
#include "scic/legacy/symtbl.hpp"
#include "scic/legacy/symtypes.hpp"
#include "scic/legacy/token.hpp"

int gSelectorIsVar;

[[nodiscard]] static ResolvedTokenSlot Immediate();
[[nodiscard]] static std::optional<RuntimeNumberOrString>
GetNumberOrStringToken(std::string_view errStr, bool stringOK);

ResolvedTokenSlot LookupTok() {
  // Get a token.  If it is an identifier, look it up in the current
  // environment and put its values in the global token slot.  Return a
  // a pointer to the symbol in the table.

  auto token = GetToken();

  if (token.type() == (sym_t)'#') return Immediate();

  if (token.type() != S_IDENT) {
    return ResolvedTokenSlot::OfToken(std::move(token));
  }

  Symbol* theSym = gSyms.lookup(token.name());

  if (!theSym) {
    return ResolvedTokenSlot::OfToken(std::move(token));
  }

  if (theSym->type == S_SELECT) {
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
        return ResolvedTokenSlot::OfToken(
            TokenSlot(S_PROP, std::string(theSym->name()), sn->ofs));
      }
    }
  }

  return ResolvedTokenSlot::OfSymbol(theSym);
}

ResolvedTokenSlot GetSymbol() {
  // Get a token that is in the symbol table.
  Symbol* theSym;
  auto token = GetToken();
  if (!(theSym = gSyms.lookup(token.name()))) {
    Severe("%s not defined.", token.name());
    return ResolvedTokenSlot::OfToken(std::move(token));
  }

  return ResolvedTokenSlot::OfSymbol(theSym);
}

std::optional<TokenSlot> GetIdent() {
  // Get an identifier.

  auto token = GetToken();
  if (!IsUndefinedIdent(token)) return std::nullopt;
  return token;
}

bool GetDefineSymbol() {
  //	gets a symbol that was previously defined

  auto token = NextToken();
  if (!token || token->type() != S_IDENT) {
    Error("Defined symbol expected");
    return false;
  }
  Symbol* sym = gSyms.lookup(token->name());
  if (!sym) return false;
  if (sym->type != S_DEFINE) {
    Error("Define expected");
    return false;
  }
  return true;
}

bool IsIdent(TokenSlot const& token) {
  if (token.type() != S_IDENT) {
    Severe("Identifier required: %s", token.name());
    return false;
  }

  return true;
}

bool IsUndefinedIdent(TokenSlot const& token) {
  if (!IsIdent(token)) return false;

  if (gSyms.lookup(token.name())) Warning("Redefinition of %s.", token.name());

  return true;
}

std::optional<int> GetNumber(std::string_view errStr) {
  auto token = GetNumberOrStringToken(errStr, false);
  if (!token) return std::nullopt;
  return std::get<int>(*token);
}

std::optional<RuntimeNumberOrString> GetNumberOrString(
    std::string_view errStr) {
  return GetNumberOrStringToken(errStr, true);
}

static std::optional<RuntimeNumberOrString> GetNumberOrStringToken(
    std::string_view errStr, bool stringOK) {
  // Get a number (or a string) from the input.

  // Get a parse node.
  auto pn = std::make_unique<PNode>(PN_EXPR);

  // Get an expression.
  Expression(pn.get(), REQUIRED);

  // If the expression is not a constant, bitch.
  pn_t type = pn->first_child()->type;
  if (type != PN_NUM && (type != PN_STRING || !stringOK)) {
    Error("%s required.", errStr);
    return std::nullopt;
  }

  // Otherwise, put the expression value into the symbol variables.
  switch (type) {
    case PN_NUM:
      return RuntimeNumberOrString(pn->first_child()->val);
    case PN_STRING:
      return RuntimeNumberOrString(*pn->first_child()->str);
    default:
      Fatal("Unexpected literal type");
  }
}

std::optional<TokenSlot> GetString(std::string_view errStr) {
  // Get a string from the input.

  auto token = GetToken();
  if (token.type() != S_STRING) {
    Severe("%s required: %s", errStr, token.name());
    return std::nullopt;
  }

  return token;
}

keyword_t Keyword(TokenSlot const& token_slot) {
  Symbol* theSym;

  if (!(theSym = gSyms.lookup(token_slot.name())) || theSym->type != S_KEYWORD)
    return K_UNDEFINED;
  else {
    return (keyword_t)theSym->val();
  }
}

void GetKeyword(keyword_t which) {
  std::string_view str;

  auto token = GetToken();
  if (Keyword(token) != which) {
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

bool IsVar(ResolvedTokenSlot const& token) {
  // return whether the current symbol is a variable

  Selector* sn;

  switch (token.type()) {
    case S_GLOBAL:
    case S_LOCAL:
    case S_TMP:
    case S_PARM:
    case S_PROP:
    case S_OPEN_BRACKET:
      return true;

    case S_SELECT:
      return gCurObj && gSelectorIsVar &&
             (sn = gCurObj->findSelectorByNum(token.val())) &&
             sn->tag == T_PROP;

    default:
      return false;
  }
}

bool IsProc(ResolvedTokenSlot const& token) {
  // If the current symbol is a procedure of some type, return the type.
  // Otherwise return false.

  return token.type() == S_PROC || token.type() == S_EXTERN;
}

bool IsObj(ResolvedTokenSlot const& token) {
  return token.type() == S_OBJ || token.type() == S_CLASS ||
         token.type() == S_IDENT || token.type() == OPEN_P || IsVar(token);
}

bool IsNumber(TokenSlot const& token) {
  return token.type() == S_NUM || token.type() == S_STRING;
}

[[nodiscard]] static ResolvedTokenSlot Immediate() {
  Symbol* theSym = nullptr;

  auto token = GetToken();
  if (token.type() == S_IDENT) {
    if (!(theSym = gSyms.lookup(token.name())) || theSym->type != S_SELECT) {
      Error("Selector required: %s", token.name());
      return ResolvedTokenSlot::OfToken(std::move(token));
    }
  }
  return ResolvedTokenSlot::OfToken(
      TokenSlot(S_NUM, absl::StrCat(theSym->val()), theSym->val()));
}
