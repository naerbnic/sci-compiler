//	toktypes.cpp	sc
// 	routines for restricting the types of tokens returned or checking on
// 	symbol/token types

#include "sol.hpp"

#include	"sc.hpp"

#include	"error.hpp"
#include	"object.hpp"
#include	"parse.hpp"
#include	"symtbl.hpp"
#include	"token.hpp"

int	selectorIsVar;

static Symbol*	Immediate();
static Bool		GetNumberOrStringToken(strptr errStr, Bool stringOK);

Symbol*
LookupTok()
{
	// Get a token.  If it is an identifier, look it up in the current
	// environment and put its values in the global token slot.  Return a
	// a pointer to the symbol in the table.

	Symbol*		theSym;
	Selector*	sn;

	GetToken();

	if (symType == (sym_t) '#')
		return Immediate();

	if (symType == S_IDENT && (theSym = syms.lookup(symStr))) {
		tokSym = *theSym;
		tokSym.name = 0;
	} else
		theSym = 0;

	if (symType == S_SELECT) {
		sn = curObj ? curObj->selectors : 0;
		if (sn) {
			// If the symbol is a property and we're in a method definition,
			//	access the symbol as a local variable.

			// A selector list is in effect -- check that the selector
			//	reference is legal (i.e. it is a property in the current
			//	selector list).
			if (!(sn = curObj->findSelector(theSym))) {
				if (!inParmList) {
					Error("Not a selector for current class/object: %s",
							theSym->name);
					theSym = 0;
				}

			} else if (sn->tag != T_LOCAL && sn->tag != T_METHOD) {
				symType = S_PROP;
				symVal = sn->ofs;
			}
		}
	}

	return theSym;
}

Bool
GetSymbol()
{
	// Get a token that is in the symbol table.

	Symbol*	theSym;

	GetToken();
	if (!(theSym = syms.lookup(symStr))) {
		Severe("%s not defined.", symStr);
		return False;
	}

	tokSym = *theSym;
	return True;
}

Bool
GetIdent()
{
	// Get an identifier.

	GetToken();
	return IsIdent();
}

Bool
GetDefineSymbol()
{
	//	gets a symbol that was previously defined

	NextToken();
	if (symType != S_IDENT) {
		Error("Defined symbol expected");
		return False;
	}
	Symbol* sym = syms.lookup(symStr);
	if (!sym)
		return False;
	if (sym->type != S_DEFINE) {
		Error("Define expected");
		return False;
	}
	return True;
}

Bool
IsIdent()
{
	if (symType != S_IDENT) {
		Severe("Identifier required: %s", symStr);
		return False;
	}

	if (syms.lookup(symStr))
		Warning("Redefinition of %s.", symStr);

	return True;
}

Bool
GetNumber(
	strptr errStr)
{
	return GetNumberOrStringToken(errStr, False);
}

Bool
GetNumberOrString(
	strptr errStr)
{
	return GetNumberOrStringToken(errStr, True);
}

static Bool
GetNumberOrStringToken(char* errStr, Bool stringOK)
{
	// Get a number (or a string) from the input.

	// Get a parse node.
	PNode* pn = New PNode(PN_EXPR);

	// Get an expression.
	Expression(pn, REQUIRED);

	// If the expression is not a constant, bitch.
	pn_t type = pn->child->type;
	if (type != PN_NUM && (type != PN_STRING || !stringOK)) {
		Error("%s required.", errStr);
		return False;
	}

	// Otherwise, put the expression value into the symbol variables.
	switch (type) {
		case PN_NUM:
			symType = S_NUM;
			break;
		case PN_STRING:
			symType = S_STRING;
			break;
	}

	symVal = pn->child->val;

	delete pn;

	return True;
}

Bool
GetString(char* errStr)
{
	// Get a string from the input.

	GetToken();
	if (symType != S_STRING) {
		Severe("%s required: %s", errStr, symStr);
		return False;
	}

	return True;
}

keyword_t
Keyword()
{
	Symbol*	theSym;

	if (!(theSym = syms.lookup(symStr)) || theSym->type != S_KEYWORD)
		return K_UNDEFINED;
	else {
		symType = S_KEYWORD;
		symVal = theSym->val;
		return (keyword_t) symVal;
	}
}

void
GetKeyword(
	keyword_t	which)
{
	strptr		str;

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

Bool
IsVar()
{
	// return whether the current symbol is a variable

	Selector*	sn;

	switch (symType) {
		case S_GLOBAL:
   	case S_LOCAL:
   	case S_TMP:
		case S_PARM:
		case S_PROP:
   	case '[':
			return True;

		case S_SELECT:
			return curObj && selectorIsVar &&
					 (sn = curObj->findSelector(&tokSym)) &&
					 sn->tag == T_PROP;

		default:
			return False;
	}
}

Bool
IsProc()
{
	// If the current symbol is a procedure of some type, return the type.
	// Otherwise return False.

	return symType == S_PROC || symType == S_EXTERN;
}

Bool
IsObj()
{
	return symType == S_OBJ || symType == S_CLASS || symType == S_IDENT ||
			 symType == OPEN_P || IsVar();
}

Bool
IsNumber()
{
	return symType == S_NUM || symType == S_STRING;
}

static Symbol*
Immediate()
{
	Symbol*	theSym;

	GetToken();
	if (symType == S_IDENT) {
		if (!(theSym = syms.lookup(symStr)) || theSym->type != S_SELECT) {
			Error("Selector required: %s", symStr);
			return 0;
		}
		tokSym = *theSym;
		tokSym.type = S_NUM;
	}
	return theSym;
}
