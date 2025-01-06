//	define.cpp

#include <stdlib.h>
#include <stdio.h>

#include "sol.hpp"

#include	"string.hpp"

#include	"sc.hpp"

#include	"anode.hpp"
#include	"compile.hpp"
#include	"define.hpp"
#include	"error.hpp"
#include	"listing.hpp"
#include	"parse.hpp"
#include	"symtbl.hpp"
#include	"token.hpp"

VarList	localVars;
VarList	globalVars;
int		maxVars = 750;

static int	InitialValue(VarList& theVars, Var* vp, int arraySize);

static Public*	publicList = NULL;
static int		publicMax = -1;
static char		tooManyVars[] = "Too many variables. Max is %d.\n";

void
VarList::kill()
{
	delete[] values;

	size = 0;
	type = VAR_NONE;
	fixups = 0;
	values = 0;
}

void
Define()
{
	// Handle a definition.
	//	define ::=	'define' symbol rest-of-expression

	if (NextToken()) {
		if (symType != S_IDENT) {
			Severe("Identifier required: %s", symStr);
			return;
		}

		Symbol* sym = syms.lookup(symStr);
		Bool newSym = sym == 0;
		if (newSym)
			sym = syms.installLocal(symStr, S_DEFINE);
		else if (sym->type != S_DEFINE)
			// This isn't just a re-'define' of the symbol, it's a change
			// in symbol type.
			Error("Redefinition of %s", sym->name);

		GetRest();

		if (!newSym) {
            strptr newString = strdup ( symStr );
            strptr oldString = strdup ( sym->str );

            // trim the two strings
            trimstr ( newString );
            trimstr ( oldString );

			if ( strcmp ( newString, oldString ) ) { 
				Warning("Redefinition of %s from %s to %s",
					sym->name, (char*) oldString, (char*) newString);
				delete[] sym->str;
				newSym = True;
			}

            free ( newString );
            free ( oldString );
		}

		if (newSym)
			sym->str = newStr(symStr);
	}
}

void
Enum()
{
	//	enum ::=	'enum' ([number] (symbol | (= symbol expr))+

	char	theNum[6];

	int val = 0;
	for (NextToken(); !CloseP(symType); NextToken()) {
		//	initializer constant?
		if (symType == S_NUM)
			val = symVal;

		else if (IsIdent()) {
			Symbol* theSym = syms.installLocal(symStr, S_DEFINE);

			//	initializer expression?
			LookupTok();
			if (symType != S_ASSIGN)
				UnGetTok();
			else {
				GetNumber("Constant expression required");
				val = symVal;
			}
			theSym->str = newStr(itoa(val, theNum, 10));
			++val;
		}
	}

	UnGetTok();
}

void
Global()
{
	// Handle a global definition.
	//
	//	global-decl ::= 	'global' glob-def+		;define a global variable
	//	glob-def 	::=	(symbol number) |
	//							open definition close

	Symbol*	theSym;
	int		size;
	int		n;
	int		offset;
	Var*		values;

	if (script) {
		Error("Globals only allowed in script 0.");
		return;
	}

	// Clear the variable array.
	values = New Var[maxVars];

	// If there are previously defined globals, copy them into the
	// variable array and free the space which they occupy.
	if (globalVars.values) {
		memcpy(values, globalVars.values, sizeof(Var)*  globalVars.size);
		delete[] globalVars.values;
	}
	size = globalVars.size;

	for (GetToken(); !CloseP(symType); GetToken()) {
		if (OpenP(symType))
			Definition();
		else if (IsIdent()) {
			// Install the symbol.
			theSym = syms.installLocal(symStr, S_GLOBAL);

			// Get the variable number and expand the size of the global block
			// if necessary.
			if (!GetNumber("Variable #"))
				break;
			offset = theSym->val = symVal;
			size = Max(offset, size);

			// Get the initial value(s) of the variable and expand the size
			// of the block if more than one value is encountered.
			n = InitialValue(globalVars, values+offset, 1);
			size = Max(size, offset + n - 1);
			if (n == -1 || size > maxVars) {
				Error(tooManyVars, maxVars);
				break;
			}
		}
	}

	// Put the information back in the variable structure.
	++size;								// account for zero-basing of variables.
	globalVars.type = VAR_GLOBAL;
	globalVars.size = size;
	n = sizeof(Var) * size;
	globalVars.values =
		(Var*) (size ? memcpy(New Var[size], values, n) : 0);

	delete[] values;
	UnGetTok();
}

void
Local()
{
	// Handle a local definition.

	//	local-decl ::=		'local' var-def+
	// 	var-def ::=		symbol |
	//							'[' symbol number ']' |
	//							open definition close

	Symbol*	theSym;
	int		size;
	int		n;
	int		arraySize;
	Var*		values;

	if (!script) {
		Error("Only globals allowed in script 0.");
		return;
	}

	if (localVars.values) {
		Error("Only one local statement allowed");
		return;
	}

	size = 0;
	values = New Var[maxVars];

	for (GetToken(); !CloseP(symType); GetToken()) {
		if (symType == (sym_t) '[') {
			if (GetIdent()) {
				theSym = syms.installLocal(symStr, S_LOCAL);
				theSym->val = size;
				if (!GetNumber("Array size"))
					break;
				arraySize = symVal;
				GetToken();
				if (symType != (sym_t) ']') {
					Severe("no closing ']' in array declaration");
					break;
				}
				n = InitialValue(localVars, values+size, arraySize);
				size += Max(n, arraySize);
				if (n == -1 || size > maxVars) {
					Error(tooManyVars, maxVars);
					break;
				}
			}

		} else if (OpenP(symType))
			Definition();

		else if (IsIdent()) {
			theSym = syms.installLocal(symStr, S_LOCAL);
			theSym->val = size;
			n = InitialValue(localVars, values + size, 1);
			size += n;
			if (n == -1 || size > maxVars) {
				Error(tooManyVars, maxVars);
				break;
			}
		}
	}

	// Put the information back in the variable structure.
	localVars.type = VAR_LOCAL;
	localVars.size = size;
	n = sizeof(Var) * size;
	localVars.values = (Var*) (size ? memcpy(New Var[size], values, n) : 0);

	delete[] values;
	UnGetTok();
}

void
Definition()
{
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

void
Extern()
{
	//	extern ::= 'extern' (symbol script# entry#)+

	Symbol*	theSym;
	Public*	theEntry;

	theEntry = 0;
	for (GetToken(); !CloseP(symType); GetToken()) {
		if (OpenP(symType))
			Definition();
		else {
			// Install the symbol in both the symbol table and the
			// externals list.
			if (!syms.lookup((strptr) (theSym = (Symbol*) symStr)))
				theSym = syms.installLocal(symStr, S_EXTERN);
			theEntry = New Public(theSym);
			theSym->ext = theEntry;

			// Get the script and entry numbers of the symbol.
			if (!GetNumber("Script #"))
				break;
			theEntry->script = symVal;

			if (!GetNumber("Entry #"))
				break;
			theEntry->entry = symVal;
		}

		theEntry = 0;
	}

	delete theEntry;

	UnGetTok();
}

void
InitPublics()
{
	Public*	pn;
	Public*	nn;

    if ( publicList ) {
        pn = publicList;

        while ( pn ) {
            nn = pn->next;
            delete pn;

            pn = nn;
        }
    }

	publicList = 0;
	publicMax = -1;
}

void
DoPublic()
{
	//	public ::= 'public' (symbol number)+

	Symbol*	theSym;
	Public*	theEntry;

	for (GetToken(); !CloseP(symType); GetToken()) {
		// Install the symbol in both the symbol table and the
		// publics list.
		if (!(theSym = syms.lookup(symStr)) || theSym->type == S_EXTERN)
			theSym = syms.installModule(symStr, (sym_t) (!theSym ? S_OBJ : S_IDENT));
		theEntry = New Public(theSym);
		theEntry->next = publicList;
		publicList = theEntry;

		if (!GetNumber("Entry #"))
			break;

		// Keep track of the maximum numbered public entry.
		theEntry->entry = symVal;
		if (symVal > publicMax)
			publicMax = symVal;
	}

	UnGetTok();

	// Generate the assembly nodes for the dispatch table.
	MakeDispatch(publicMax);
}

Symbol*
FindPublic(
	int	n)
{
	// Return a pointer to the symbol which is entry number 'n' in the
	// dispatch table.

	Public*	pp;

	for (pp = publicList ; pp && pp->entry != (uint) n ; pp = pp->next)
		;

	return pp ? pp->sym : 0;
}

static int
InitialValue(
	VarList&	theVars,
	Var*		vp,
	int		arraySize)
{
	// 'vp' points to the variable(s) to initialize int 'theVars'.  Fill it in
	//	with any initial values, returning 1 if there are no initial values, the
	// number of initial values otherwise.  Syntax is
	//		= num | [num ...]
	// 'arraySize' is the declared size of the variable array.  If the initial
	// value is not a set of values ('num' rather than  '[num ...]'), the array
	// is filled with the single value passed.

	int		n, i;

	// See if there are initial values.  Return 1 if not (by default, there
	// is one initial value of 0 for all variable declarations).
	LookupTok();
	if (symType != S_ASSIGN) {
		UnGetTok();
		return 1;
	}

	// See if the initialization is for an array.  If not, just get one
	// initial value and return.
	GetToken();
	if (symType != (sym_t) '[') {
		UnGetTok();
		if (theVars.size+1 > maxVars)
			return -1;
		GetNumberOrString("Initial value");
		for (n = 0, i = 0 ; n < arraySize ; ++n, ++i) {
			switch (symType) {
				case S_NUM:
					break;
				default:
					++theVars.fixups;
			}
			vp->type = symType;
			vp++->value = symVal;
		}
		return i;
	}

	// Read an array of initial values and return the number defined.
	for (n = 0, GetToken(); symType != (sym_t) ']'; ++n, GetToken()) {
		UnGetTok();
		if (theVars.size + n > maxVars)
			return -1;
		GetNumberOrString("Initial value");
		vp->type = symType;
		switch (symType) {
			case S_NUM:
				break;
			default:
				++theVars.fixups;
		}
		vp++->value = symVal;
	}
	return n;
}
