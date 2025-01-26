//	define.cpp

#include "define.hpp"

#include <stdio.h>
#include <stdlib.h>

#include <sstream>
#include <string_view>

#include "anode.hpp"
#include "compile.hpp"
#include "error.hpp"
#include "parse.hpp"
#include "sc.hpp"
#include "sol.hpp"
#include "string.hpp"
#include "symtbl.hpp"
#include "token.hpp"

VarList localVars;
VarList globalVars;
int maxVars = 750;

static int InitialValue(VarList& theVars, int offset, int arraySize);

static std::deque<std::unique_ptr<Public>> publicList;
static int publicMax = -1;
static constexpr std::string_view tooManyVars =
    "Too many variables. Max is %d.\n";

namespace {

char* newStrFromInt(int val) {
  std::stringstream stream;
  stream << val;
  return newStr(stream.str().c_str());
}

}  // namespace

void VarList::kill() {
  type = VAR_NONE;
  values.clear();
}

void Define() {
  // Handle a definition.
  //	define ::=	'define' symbol rest-of-expression

  if (NextToken()) {
    if (symType != S_IDENT) {
      Severe("Identifier required: %s", symStr);
      return;
    }

    Symbol* sym = syms.lookup(symStr);
    bool newSym = sym == 0;
    if (newSym)
      sym = syms.installLocal(symStr, S_DEFINE);
    else if (sym->type != S_DEFINE)
      // This isn't just a re-'define' of the symbol, it's a change
      // in symbol type.
      Error("Redefinition of %s", sym->name());

    GetRest();

    if (!newSym) {
      char* newString = newStr(symStr);
      char* oldString = newStr(sym->str());

      // trim the two strings
      trimstr(newString);
      trimstr(oldString);

      if (strcmp(newString, oldString)) {
        Warning("Redefinition of %s from %s to %s", sym->name(),
                (char*)oldString, (char*)newString);
        delete[] sym->str();
        newSym = True;
      }

      free(newString);
      free(oldString);
    }

    if (newSym) sym->setStr(newStr(symStr));
  }
}

void Enum() {
  //	enum ::=	'enum' ([number] (symbol | (= symbol expr))+

  int val = 0;
  for (NextToken(); !CloseP(symType); NextToken()) {
    //	initializer constant?
    if (symType == S_NUM)
      val = symVal;

    else if (IsUndefinedIdent()) {
      Symbol* theSym = syms.installLocal(symStr, S_DEFINE);

      //	initializer expression?
      LookupTok();
      if (symType != S_ASSIGN)
        UnGetTok();
      else {
        GetNumber("Constant expression required");
        val = symVal;
      }
      theSym->setStr(newStrFromInt(val));
      ++val;
    }
  }

  UnGetTok();
}

void GlobalDecl() {
  // Handle a forward definition of global variables.

  for (GetToken(); !CloseP(symType); GetToken()) {
    if (!IsIdent()) {
      Severe("Global variable name expected. Got: %s", symStr);
      break;
    }
    std::string varName = symStr;
    if (!GetNumber("Variable #")) break;

    // We only install into the symbol table for globals. We do not add
    // global variables to the globalVars list. That still has to be
    // done by Script0.
    Symbol* theSym = syms.lookup(varName.c_str());
    if (theSym) {
      if (theSym->type != S_GLOBAL) {
        Error("Redefinition of %s as a global.", theSym->name());
        break;
      }

      if (theSym->val() != symVal) {
        Error(
            "Redefinition of %s with different global index (%d expected, %d "
            "found).",
            theSym->name(), theSym->val(), symVal);
        break;
      }
    } else {
      // Install the symbol.
      theSym = syms.installLocal(varName.c_str(), S_GLOBAL);
      theSym->setVal(symVal);
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

  if (script) {
    Error("Globals only allowed in script 0.");
    return;
  }

  // If there are previously defined globals, keep them in the globals
  // list.

  for (GetToken(); !CloseP(symType); GetToken()) {
    if (OpenP(symType))
      Definition();
    else if (IsIdent()) {
      std::string varName = symStr;
      if (!GetNumber("Variable #")) break;

      // Try to get the symbol from the symbol table.
      Symbol* theSym = syms.lookup(varName.c_str());
      if (theSym) {
        if (theSym->type != S_GLOBAL) {
          Error("Redefinition of %s as a global.", theSym->name());
          break;
        }

        if (theSym->val() != symVal) {
          Error(
              "Redefinition of %s with different global index (%d expected, %d "
              "found).",
              theSym->name(), theSym->val(), symVal);
          break;
        }
      } else {
        // Install the symbol.
        theSym = syms.installLocal(varName.c_str(), S_GLOBAL);
        theSym->setVal(symVal);
      }
      offset = theSym->val();

      // Get the initial value(s) of the variable and expand the size
      // of the block if more than one value is encountered.
      n = InitialValue(globalVars, offset, 1);
      if (n == -1 || globalVars.values.size() > maxVars) {
        Error(tooManyVars, maxVars);
        break;
      }
    }
  }

  // Put the information back in the variable structure.
  globalVars.type = VAR_GLOBAL;

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

  if (!script) {
    Error("Only globals allowed in script 0.");
    return;
  }

  if (!localVars.values.empty()) {
    Error("Only one local statement allowed");
    return;
  }

  size = 0;

  for (GetToken(); !CloseP(symType); GetToken()) {
    if (symType == S_OPEN_BRACKET) {
      if (GetIdent()) {
        theSym = syms.installLocal(symStr, S_LOCAL);
        theSym->setVal(size);
        if (!GetNumber("Array size")) break;
        arraySize = symVal;
        GetToken();
        if (symType != (sym_t)']') {
          Severe("no closing ']' in array declaration");
          break;
        }
        n = InitialValue(localVars, size, arraySize);
        size += Max(n, arraySize);
        if (n == -1 || size > maxVars) {
          Error(tooManyVars, maxVars);
          break;
        }
      }

    } else if (OpenP(symType))
      Definition();

    else if (IsUndefinedIdent()) {
      theSym = syms.installLocal(symStr, S_LOCAL);
      theSym->setVal(size);
      n = InitialValue(localVars, size, 1);
      size += n;
      if (n == -1 || size > maxVars) {
        Error(tooManyVars, maxVars);
        break;
      }
    }
  }

  // Put the information back in the variable structure.
  localVars.type = VAR_LOCAL;

  UnGetTok();
}

void Definition() {
  GetToken();
  switch (Keyword()) {
    case K_DEFINE:
      Define();
      break;

    case K_ENUM:
      Enum();
      break;

    default:
      Severe("define or enum expected: %s", symStr);
  }
  CloseBlock();
}

void Extern() {
  //	extern ::= 'extern' (symbol script# entry#)+

  Symbol* theSym;

  for (GetToken(); !CloseP(symType); GetToken()) {
    if (OpenP(symType))
      Definition();
    else {
      // Install the symbol in both the symbol table and the
      // externals list.
      if (!syms.lookup((strptr)(theSym = (Symbol*)symStr)))
        theSym = syms.installLocal(symStr, S_EXTERN);
      auto entry = std::make_unique<Public>(theSym);
      auto* theEntry = entry.get();
      theSym->setExt(std::move(entry));

      // Get the script and entry numbers of the symbol.
      if (!GetNumber("Script #")) break;
      theEntry->script = symVal;

      if (!GetNumber("Entry #")) break;
      theEntry->entry = symVal;
    }
  }

  UnGetTok();
}

void InitPublics() {
  publicList.clear();
  publicMax = -1;
}

void DoPublic() {
  //	public ::= 'public' (symbol number)+

  Symbol* theSym;

  for (GetToken(); !CloseP(symType); GetToken()) {
    // Install the symbol in both the symbol table and the
    // publics list.
    if (!(theSym = syms.lookup(symStr)) || theSym->type == S_EXTERN)
      theSym = syms.installModule(symStr, (sym_t)(!theSym ? S_OBJ : S_IDENT));
    auto theEntry = std::make_unique<Public>(theSym);
    auto* entryPtr = theEntry.get();
    publicList.push_front(std::move(theEntry));

    if (!GetNumber("Entry #")) break;

    // Keep track of the maximum numbered public entry.
    entryPtr->entry = symVal;
    if (symVal > publicMax) publicMax = symVal;
  }

  UnGetTok();

  // Generate the assembly nodes for the dispatch table.
  MakeDispatch(publicMax);
}

Symbol* FindPublic(int n) {
  // Return a pointer to the symbol which is entry number 'n' in the
  // dispatch table.

  for (auto const& entry : publicList) {
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
  LookupTok();
  if (symType != S_ASSIGN) {
    UnGetTok();
    return 1;
  }

  if (offset + arraySize > maxVars) return -1;

  if (theVars.values.size() < (offset + arraySize)) {
    theVars.values.resize(offset + arraySize);
  }

  // See if the initialization is for an array.  If not, just get one
  // initial value and return.
  GetToken();
  if (symType != (sym_t)'[') {
    UnGetTok();
    GetNumberOrString("Initial value");
    for (int i = 0; i < arraySize; ++i) {
      Var* vp = &theVars.values[offset + i];
      if (vp->type != (sym_t)VAR_NONE) {
        Error("Redefinition of index %d", offset + i);
      }
      vp->type = symType;
      vp->value = symVal;
    }
    return arraySize;
  }

  int n;
  // Read an array of initial values and return the number defined.
  for (n = 0, GetToken(); symType != (sym_t)']'; ++n, GetToken()) {
    UnGetTok();
    GetNumberOrString("Initial value");
    Var* vp = &theVars.values[offset + n];
    vp->type = symType;
    vp++->value = symVal;
  }
  return n;
}
