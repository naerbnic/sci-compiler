//	define.cpp

#include "scic/define.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "scic/anode.hpp"
#include "scic/compile.hpp"
#include "scic/config.hpp"
#include "scic/error.hpp"
#include "scic/parse.hpp"
#include "scic/parse_context.hpp"
#include "scic/public.hpp"
#include "scic/sc.hpp"
#include "scic/symbol.hpp"
#include "scic/symtbl.hpp"
#include "scic/symtypes.hpp"
#include "scic/token.hpp"
#include "scic/toktypes.hpp"
#include "scic/varlist.hpp"

static int InitialValue(VarList& theVars, int offset, int arraySize);

static constexpr std::string_view tooManyVars =
    "Too many variables. Max is %d.\n";

void Define() {
  // Handle a definition.
  //	define ::=	'define' symbol rest-of-expression

  auto token = NextToken();
  if (token) {
    if (token->type() != S_IDENT) {
      Severe("Identifier required: %s", token->name());
      return;
    }

    Symbol* sym = gParseContext.syms.lookup(token->name());
    bool newSym = sym == 0;
    if (newSym)
      sym = gParseContext.syms.installLocal(token->name(), S_DEFINE);
    else if (sym->type != S_DEFINE)
      // This isn't just a re-'define' of the symbol, it's a change
      // in symbol type.
      Error("Redefinition of %s", sym->name());

    auto rest = GetRest();

    if (!newSym) {
      std::string_view newString =
          absl::StripAsciiWhitespace(rest ? rest->name() : "");
      std::string_view oldString = absl::StripAsciiWhitespace(sym->str());

      if (newString != oldString) {
        Warning("Redefinition of %s from %s to %s", sym->name(), oldString,
                newString);
        newSym = true;
      }
    }

    if (newSym) sym->setStr(std::string(rest ? rest->name() : ""));
  }
}

void Enum() {
  //	enum ::=	'enum' ([number] (symbol | (= symbol expr))+

  int val = 0;
  for (auto token = NextToken(); token && !CloseP(token->type());
       token = NextToken()) {
    //	initializer constant?
    if (token->type() == S_NUM) {
      val = token->val();
    } else if (IsUndefinedIdent(*token)) {
      Symbol* theSym = gParseContext.syms.installLocal(token->name(), S_DEFINE);

      //	initializer expression?
      auto expr = LookupTok();
      if (expr.type() != S_ASSIGN)
        UnGetTok();
      else {
        auto value = GetNumber("Constant expression required");
        val = value.value_or(0);
      }
      theSym->setStr(absl::StrCat(val));
      ++val;
    }
  }

  UnGetTok();
}

void GlobalDecl() {
  // Handle a forward definition of global variables.

  for (auto token = GetToken(); !CloseP(token.type()); token = GetToken()) {
    if (!IsIdent(token)) {
      Severe("Global variable name expected. Got: %s", token.name());
      break;
    }
    std::string varName(token.name());
    auto var_num = GetNumber("Variable #");
    if (!var_num) break;

    // We only install into the symbol table for globals. We do not add
    // global variables to the gParseContext.globalVars list. That still has to be
    // done by Script0.
    Symbol* theSym = gParseContext.syms.lookup(varName);
    if (theSym) {
      if (theSym->type != S_GLOBAL) {
        Error("Redefinition of %s as a global.", theSym->name());
        break;
      }

      if (theSym->val() != *var_num) {
        Error(
            "Redefinition of %s with different global index (%d expected, %d "
            "found).",
            theSym->name(), theSym->val(), *var_num);
        break;
      }
    } else {
      // Install the symbol.
      theSym = gParseContext.syms.installLocal(varName, S_GLOBAL);
      theSym->setVal(*var_num);
    }
  }

  UnGetTok();
}

void Global() {
  // Handle a global definition.
  //
  //	global-decl ::= 	'global' glob-def+		;define a global
  // variable 	glob-def 	::=	(symbol number) |
  // open definition close

  int n;
  int offset;

  if (gScript) {
    Error("Globals only allowed in script 0.");
    return;
  }

  // If there are previously defined globals, keep them in the globals
  // list.

  for (auto token = GetToken(); !CloseP(token.type()); token = GetToken()) {
    if (OpenP(token.type()))
      Definition();
    else if (IsIdent(token)) {
      std::string varName(token.name());
      auto var_num = GetNumber("Variable #");
      if (!var_num) break;

      // Try to get the symbol from the symbol table.
      Symbol* theSym = gParseContext.syms.lookup(varName);
      if (theSym) {
        if (theSym->type != S_GLOBAL) {
          Error("Redefinition of %s as a global.", theSym->name());
          break;
        }

        if (theSym->val() != *var_num) {
          Error(
              "Redefinition of %s with different global index (%d expected, %d "
              "found).",
              theSym->name(), theSym->val(), *var_num);
          break;
        }
      } else {
        // Install the symbol.
        theSym = gParseContext.syms.installLocal(varName, S_GLOBAL);
        theSym->setVal(*var_num);
      }
      offset = theSym->val();

      // Get the initial value(s) of the variable and expand the size
      // of the block if more than one value is encountered.
      n = InitialValue(gParseContext.globalVars, offset, 1);
      if (n == -1 || gParseContext.globalVars.values.size() > gConfig->maxVars) {
        Error(tooManyVars, gConfig->maxVars);
        break;
      }
    }
  }

  // Put the information back in the variable structure.
  gParseContext.globalVars.type = VAR_GLOBAL;

  UnGetTok();
}

void Local() {
  // Handle a local definition.

  //	local-decl ::=		'local' var-def+
  // 	var-def ::=		symbol |
  //							'[' symbol number ']' |
  //							open definition close

  Symbol* theSym;
  int size;
  int n;
  int arraySize;

  if (!gScript) {
    Error("Only globals allowed in script 0.");
    return;
  }

  if (!gParseContext.localVars.values.empty()) {
    Error("Only one local statement allowed");
    return;
  }

  size = 0;

  for (auto token = GetToken(); !CloseP(token.type()); token = GetToken()) {
    if (token.type() == S_OPEN_BRACKET) {
      auto ident = GetIdent();
      if (ident) {
        theSym = gParseContext.syms.installLocal(ident->name(), S_LOCAL);
        theSym->setVal(size);
        auto array_size = GetNumber("Array size");
        if (!array_size) break;
        arraySize = *array_size;
        auto close = GetToken();
        if (close.type() != (sym_t)']') {
          Severe("no closing ']' in array declaration");
          break;
        }
        n = InitialValue(gParseContext.localVars, size, arraySize);
        size += std::max(n, arraySize);
        if (n == -1 || (std::size_t)(size) > gConfig->maxVars) {
          Error(tooManyVars, gConfig->maxVars);
          break;
        }
      }

    } else if (OpenP(token.type()))
      Definition();

    else if (IsUndefinedIdent(token)) {
      theSym = gParseContext.syms.installLocal(token.name(), S_LOCAL);
      theSym->setVal(size);
      n = InitialValue(gParseContext.localVars, size, 1);
      size += n;
      if (n == -1 || (std::size_t)(size) > gConfig->maxVars) {
        Error(tooManyVars, gConfig->maxVars);
        break;
      }
    }
  }

  // Put the information back in the variable structure.
  gParseContext.localVars.type = VAR_LOCAL;

  UnGetTok();
}

void Definition() {
  auto token = GetToken();
  switch (Keyword(token)) {
    case K_DEFINE:
      Define();
      break;

    case K_ENUM:
      Enum();
      break;

    default:
      Severe("define or enum expected: %s", token.name());
  }
  CloseBlock();
}

void Extern() {
  //	extern ::= 'extern' (symbol script# entry#)+

  for (auto token = GetToken(); !CloseP(token.type()); token = GetToken()) {
    if (OpenP(token.type()))
      Definition();
    else {
      // Install the symbol in both the symbol table and the
      // externals list.
      Symbol* theSym = gParseContext.syms.lookup(token.name());
      if (!theSym) {
        theSym = gParseContext.syms.installLocal(token.name(), S_EXTERN);
      }
      auto entry = std::make_unique<Public>(theSym);
      auto* theEntry = entry.get();
      theSym->setExt(std::move(entry));

      // Get the script and entry numbers of the symbol.
      auto script_num = GetNumber("Script #");
      if (!script_num) break;
      theEntry->script = *script_num;

      auto entry_num = GetNumber("Entry #");
      if (!entry_num) break;
      theEntry->entry = *entry_num;
    }
  }

  UnGetTok();
}

void InitPublics() {
  gParseContext.publicList.clear();
  gParseContext.publicMax = -1;
}

void DoPublic() {
  //	public ::= 'public' (symbol number)+

  Symbol* theSym;

  for (auto token = GetToken(); !CloseP(token.type()); token = GetToken()) {
    // Install the symbol in both the symbol table and the
    // publics list.
    if (!(theSym = gParseContext.syms.lookup(token.name())) || theSym->type == S_EXTERN)
      theSym =
          gParseContext.syms.installModule(token.name(), (sym_t)(!theSym ? S_OBJ : S_IDENT));
    auto theEntry = std::make_unique<Public>(theSym);
    auto* entryPtr = theEntry.get();
    gParseContext.publicList.push_front(std::move(theEntry));

    auto entry_num = GetNumber("Entry #");
    if (!entry_num) break;

    // Keep track of the maximum numbered public entry.
    entryPtr->entry = *entry_num;
    if (*entry_num > gParseContext.publicMax) gParseContext.publicMax = *entry_num;
  }

  UnGetTok();

  // Generate the assembly nodes for the dispatch table.
  MakeDispatch(gParseContext.publicMax);
}

Symbol* FindPublic(int n) {
  // Return a pointer to the symbol which is entry number 'n' in the
  // dispatch table.

  for (auto const& entry : gParseContext.publicList) {
    if (entry->entry == (uint32_t)n) return entry->sym;
  }

  return nullptr;
}

static int InitialValue(VarList& theVars, int offset, int arraySize) {
  // 'vp' points to the variable(s) to initialize int 'theVars'.  Fill it in
  //	with any initial values, returning 1 if there are no initial values, the
  // number of initial values otherwise.  Syntax is
  //		= num | [num ...]
  // 'arraySize' is the declared size of the variable array.  If the initial
  // value is not a set of values ('num' rather than  '[num ...]'), the array
  // is filled with the single value passed.

  // See if there are initial values.  Return 1 if not (by default, there
  // is one initial value of 0 for all variable declarations).
  auto slot = LookupTok();
  if (slot.type() != S_ASSIGN) {
    UnGetTok();
    return 1;
  }

  if ((std::size_t)(offset + arraySize) > gConfig->maxVars) return -1;

  if (theVars.values.size() < (std::size_t)(offset + arraySize)) {
    theVars.values.resize(offset + arraySize);
  }

  // See if the initialization is for an array.  If not, just get one
  // initial value and return.
  auto token = GetToken();
  if (token.type() != (sym_t)'[') {
    UnGetTok();
    auto value = GetNumberOrString("Initial value");
    for (int i = 0; i < arraySize; ++i) {
      Var* vp = &theVars.values[offset + i];
      if (vp->type != (sym_t)VAR_NONE) {
        Error("Redefinition of index %d", offset + i);
      }
      if (value) {
        vp->type = value->type();
        vp->value = value->val();
      }
    }
    return arraySize;
  }

  int n;
  // Read an array of initial values and return the number defined.
  for (n = 0, token = GetToken(); token.type() != (sym_t)']';
       ++n, token = GetToken()) {
    UnGetTok();
    auto value = GetNumberOrString("Initial value");
    Var* vp = &theVars.values[offset + n];
    if (value) {
      vp->type = value->type();
      vp++->value = value->val();
    }
  }
  return n;
}
