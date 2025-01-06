//	proc.cpp		sc

#include "sol.hpp"

#include	"sc.hpp"

#include	"compile.hpp"
#include	"error.hpp"
#include	"object.hpp"
#include	"parse.hpp"
#include	"token.hpp"
#include	"symtbl.hpp"

Bool	  	inParmList;

static PNode*	_CallDef(sym_t theType);
static int		ParameterList();
static void		NewParm(int n, sym_t type);
static void		AddRest(int ofs);

void
Procedure()
{
	// 	procedure ::= 'procedure' call-def [expression+]
	//	OR
	//		procedure ::= procedure procedure-name+

	PNode*	theNode;
	Symbol*	theSym;
	SymTbl*	theSymTbl;

	GetToken();
	UnGetTok();
	if (symType == OPEN_P) {
		// Then a procedure definition.
		theSymTbl = syms.add(ST_MINI);

		theNode = CallDef(S_PROC);
		if (theNode) {
			ExprList(theNode, OPTIONAL);
			CompileCode(theNode);
		}

		syms.deactivate(theSymTbl);

	} else {
		// A procedure declaration.
		for (GetToken(); !CloseP(symType); GetToken()) {
			if (symType == S_IDENT)
				theSym = syms.installLocal(symStr, S_PROC);
			theSym->val = UNDEFINED;
		}
		UnGetTok();
	}
}

PNode	*
CallDef(
	sym_t theType)
{
	// call-def ::= open _call-def close

	PNode*	theNode;

	if (!OpenBlock()) {
		UnGetTok();
		Error("expected opening parenthesis or brace.");
		return 0;
	}

	theNode = _CallDef(theType);
	CloseBlock();

	return theNode;
}

static PNode*
_CallDef(
	sym_t theType)
{
	// _call-def ::= symbol [variable+] [&tmp variable+]

	Symbol*		theProc;
	PNode	*		theNode;
	Selector*	sn;

	GetToken();
	theProc = syms.lookup(symStr);
	switch (theType) {
		case S_PROC:
			if (!theProc)
				theProc = syms.installModule(symStr, theType);

			else if (theProc->type != S_PROC || theProc->val != UNDEFINED) {
				Severe("%s is already defined.", symStr);
				return 0;
			}

			theProc->val = DEFINED;
			break;

		case S_SELECT:
			if (!theProc ||
				 !(sn = curObj->findSelector(theProc)) ||
				 IsProperty(sn)) {
				Severe("%s is not a method for class %s", symStr,curObj->sym->name);
				return 0;
			}
	}

	theNode = New PNode(theType == S_SELECT ? PN_METHOD : PN_PROC);
	theNode->sym = theProc;
	theNode->val = ParameterList();	// number of temporary variables

	return theNode;
}

static int
ParameterList()
{
	// parameter-list ::= [variable+] [&tmp variable+]

	int	parmOfs;
	sym_t	parmType;

	parmOfs = 1;
	parmType = S_PARM;

	inParmList = True;
	for (LookupTok(); !CloseP(symType); LookupTok()) {
		if (symType == S_KEYWORD && symVal == K_TMP) {
			// Now defining temporaries -- set 'rest of argument' value.
			AddRest(parmOfs);
			parmOfs = 0;
			parmType = S_TMP;

		} else if (symType == S_IDENT)
			// A parameter or tmp variable definition.
			NewParm(parmOfs++, parmType);

		else if (symType == (sym_t) '[') {
			// An array parameter or tmp variable.
			if (!GetIdent())
				break;
			NewParm(parmOfs, parmType);
			if (!GetNumber("array size"))
				return 0;
			parmOfs += symVal;
			GetToken();
			if (symType != (sym_t) ']') {
				Error("expecting closing ']': %s.", symStr);
				UnGetTok();
			}

		} else if (symType == S_SELECT) {
			if (curObj && curObj->findSelector(&tokSym))
				Error("%s is a selector for current object.", symStr);
			else 
				syms.installLocal(symStr, parmType)->val = parmOfs++;

		} else
			Error("Non-identifier in parameter list: %s", symStr);
	}
	
	if (parmType == S_PARM)
		AddRest(parmOfs);

	inParmList = False;

	UnGetTok();

	// Return the number of temporary variables.
	return parmType == S_PARM ? 0 : parmOfs;
}

static void
NewParm(
	int	n,
	sym_t	type)
{
	Symbol*	theSym;

	if (syms.lookup(symStr))
		Warning("Redefinition of '%s'.", symStr);
	theSym = syms.installLocal(symStr, type);
	theSym->val = n;
}

static void
AddRest(
	int	ofs)
{
	Symbol*	theSym;

	theSym = syms.installLocal("&rest", S_REST);
	theSym->val = ofs;
}
