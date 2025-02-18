//	define.cpp

#include "scic/define.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "scic/anode_impls.hpp"
#include "scic/compile.hpp"
#include "scic/config.hpp"
#include "scic/error.hpp"
#include "scic/global_compiler.hpp"
#include "scic/parse.hpp"
#include "scic/public.hpp"
#include "scic/sc.hpp"
#include "scic/symbol.hpp"
#include "scic/symtbl.hpp"
#include "scic/symtypes.hpp"
#include "scic/token.hpp"
#include "scic/toktypes.hpp"
#include "util/types/overload.hpp"

static bool InitialValue(int offset, int arraySize);

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

    Symbol* sym = gSyms.lookup(token->name());
    bool newSym = sym == 0;
    if (newSym)
      sym = gSyms.installLocal(token->name(), S_DEFINE);
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
      Symbol* theSym = gSyms.installLocal(token->name(), S_DEFINE);

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
    // global variables to the gGlobalVars list. That still has to be
    // done by Script0.
    Symbol* theSym = gSyms.lookup(varName);
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
      theSym = gSyms.installLocal(varName, S_GLOBAL);
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
      Symbol* theSym = gSyms.lookup(varName);
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
        theSym = gSyms.installLocal(varName, S_GLOBAL);
        theSym->setVal(*var_num);
      }
      offset = theSym->val();

      // Get the initial value(s) of the variable and expand the size
      // of the block if more than one value is encountered.
      if (!InitialValue(offset, 1)) {
        Error(tooManyVars, gConfig->maxVars);
        break;
      }
    }
  }

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
  int arraySize;

  if (!gScript) {
    Error("Only globals allowed in script 0.");
    return;
  }

  if (gSc->NumVars() > 0) {
    Error("Only one local statement allowed");
    return;
  }

  size = 0;

  for (auto token = GetToken(); !CloseP(token.type()); token = GetToken()) {
    if (token.type() == S_OPEN_BRACKET) {
      auto ident = GetIdent();
      if (ident) {
        theSym = gSyms.installLocal(ident->name(), S_LOCAL);
        theSym->setVal(size);
        auto array_size = GetNumber("Array size");
        if (!array_size) break;
        arraySize = *array_size;
        auto close = GetToken();
        if (close.type() != (sym_t)']') {
          Severe("no closing ']' in array declaration");
          break;
        }
        size += arraySize;
        if (!InitialValue(size, arraySize)) {
          Error(tooManyVars, gConfig->maxVars);
          break;
        }
      }

    } else if (OpenP(token.type()))
      Definition();

    else if (IsUndefinedIdent(token)) {
      theSym = gSyms.installLocal(token.name(), S_LOCAL);
      theSym->setVal(size);
      size += 1;
      if (!InitialValue(size, 1)) {
        Error(tooManyVars, gConfig->maxVars);
        break;
      }
    }
  }

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
      Symbol* theSym = gSyms.lookup(token.name());
      if (!theSym) {
        theSym = gSyms.installLocal(token.name(), S_EXTERN);
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

void DoPublic() {
  //	public ::= 'public' (symbol number)+

  Symbol* theSym;

  PublicList publicList;

  for (auto token = GetToken(); !CloseP(token.type()); token = GetToken()) {
    // Install the symbol in both the symbol table and the
    // publics list.
    if (!(theSym = gSyms.lookup(token.name())) || theSym->type == S_EXTERN)
      theSym =
          gSyms.installModule(token.name(), (sym_t)(!theSym ? S_OBJ : S_IDENT));
    auto theEntry = std::make_unique<Public>(theSym);
    auto* entryPtr = theEntry.get();
    publicList.push_front(std::move(theEntry));

    auto entry_num = GetNumber("Entry #");
    if (!entry_num) break;

    // Keep track of the maximum numbered public entry.
    entryPtr->entry = *entry_num;
  }

  UnGetTok();

  // Generate the assembly nodes for the dispatch table.
  gSc->MakeDispatch(publicList);
}

Symbol* FindPublic(PublicList const& publicList, int n) {
  // Return a pointer to the symbol which is entry number 'n' in the
  // dispatch table.

  for (auto const& entry : publicList) {
    if (entry->entry == (uint32_t)n) return entry->sym;
  }

  return nullptr;
}

static bool InitialValue(int offset, int arraySize) {
  // 'vp' points to the variable(s) to initialize int 'theVars'.  Fill it in
  //	with any initial values, returning 1 if there are no initial values, the
  // number of initial values otherwise.  Syntax is
  //		= num | [num ...]
  // 'arraySize' is the declared size of the variable array.  If the initial
  // value is not a set of values ('num' rather than  '[num ...]'), the array
  // is filled with the single value passed.

  if ((std::size_t)(offset + arraySize) > gConfig->maxVars) return false;

  // See if there are initial values.  Return 1 if not (by default, there
  // is one initial value of 0 for all variable declarations).
  auto slot = LookupTok();
  if (slot.type() != S_ASSIGN) {
    UnGetTok();
    for (int i = 0; i < arraySize; ++i) {
      gSc->SetIntVar(offset + i, 0);
    }
    return true;
  }

  // See if the initialization is for an array.  If not, just get one
  // initial value and return.
  auto token = GetToken();
  if (token.type() != (sym_t)'[') {
    UnGetTok();
    auto value = GetNumberOrString("Initial value");
    for (int i = 0; i < arraySize; ++i) {
      if (!value) {
        gSc->SetIntVar(offset + i, 0);
        continue;
      }
      std::visit(util::Overload(
                     [&](int num) { gSc->SetIntVar(offset + i, num); },
                     [&](ANText* text) { gSc->SetTextVar(offset + i, text); }),
                 *value);
    }
    return arraySize;
  }

  int n;
  // Read an array of initial values and return the number defined.
  for (n = 0, token = GetToken(); token.type() != (sym_t)']';
       ++n, token = GetToken()) {
    UnGetTok();
    auto value = GetNumberOrString("Initial value");
    if (!value) {
      gSc->SetIntVar(offset + n, 0);
      continue;
    }
    std::visit(util::Overload(
                   [&](int num) { gSc->SetIntVar(offset + n, num); },
                   [&](ANText* text) { gSc->SetTextVar(offset + n, text); }),
               *value);
  }
  return n;
}
