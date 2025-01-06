//	class.cpp
// 	code to deal with classes.

#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>

#include "sol.hpp"

//#include	"jeff.hpp"
//#include	"char.hpp"
#include	"string.hpp"

#include	"sc.hpp"

#include	"error.hpp"
#include	"opcodes.hpp"
#include	"object.hpp"
#include	"parse.hpp"
#include	"resource.hpp"
#include	"symtbl.hpp"
#include	"token.hpp"

#define	MAXCLASSES	512				// Maximum number of classes

Class*	classes[MAXCLASSES];
int		maxClassNum = -1;

static void		DefClassItems(Class* theClass, int what);

void
InstallObjects()
{
	// Install 'RootObj' as the root of the class system.
	Symbol *sym = syms.installClass("RootObj");
	Class* rootClass = New Class;
	sym->obj = rootClass;
	rootClass->sym = sym;
	rootClass->script = KERNEL;
	rootClass->num = -1;

	// Install the root class' selectors in the symbol table and add them
	// to the root class.
	InstallSelector("-objID-", SEL_OBJID);
	if (sym = syms.lookup("-objID-"))
		rootClass->addSelector(sym, T_PROP)->val = 0x1234;

	InstallSelector("-size-", SEL_SIZE);
	if (sym = syms.lookup("-size-"))
		rootClass->addSelector(sym, T_PROP);

	InstallSelector("-propDict-", SEL_PROPDICT);
	if (sym = syms.lookup("-propDict-"))
		rootClass->addSelector(sym, T_PROPDICT);

	InstallSelector("-methDict-", SEL_METHDICT);
	if (sym = syms.lookup("-methDict-"))
		rootClass->addSelector(sym, T_METHDICT);

	InstallSelector("-classScript-", SEL_CLASS_SCRIPT);
	if (sym = syms.lookup("-classScript-"))
		rootClass->addSelector(sym, T_PROP)->val = 0;

	InstallSelector("-script-", SEL_SCRIPT);
	if (sym = syms.lookup("-script-"))
		rootClass->addSelector(sym, T_PROP);

	InstallSelector("-super-", SEL_SUPER);
	if (sym = syms.lookup("-super-"))
		rootClass->addSelector(sym, T_PROP)->val = -1;

	InstallSelector("-info-", SEL_INFO);
	if (sym = syms.lookup("-info-"))
		rootClass->addSelector(sym, T_PROP)->val = CLASSBIT;

	// Install 'self' and 'super' as objects.
	sym = syms.installGlobal("self", S_OBJ);
	sym->val = (int) OBJ_SELF;
	sym = syms.installGlobal("super", S_CLASS);
	sym->val = (int) OBJ_SUPER;
}

void
DefineClass()
{
	// class-def ::=	'classdef' symbol 'kindof' ('RootObj' | class-name)
	//				'script#' number 'class#' number 'super#' number 'file#' string
	//				(property-list | method-list)*

	// Get and install the name of this class.  The class is installed in
	// the class symbol table, which will be used to write out classdefs
	// later.
	Symbol *sym = LookupTok();
	if (!sym)
		sym = syms.installClass(symStr);

	else if (symType == S_IDENT || symType == S_OBJ) {
		syms.del(symStr);
		sym = syms.installClass(symStr);

	} else {
		Severe("Redefinition of %s.", symStr);
		return;
	}

	// Get the script, class, super-class numbers, and file name.
	GetKeyword(K_SCRIPTNUM);
	GetNumber("Script #");
	int scriptNum = symVal;
	GetKeyword(K_CLASSNUM);
	GetNumber("Class #");
	int classNum = symVal;
	GetKeyword(K_SUPER);
	GetNumber("Super #");
	int superNum = symVal;
	GetKeyword(K_FILE);
	GetString("File name");
	strptr superFile = newStr(symStr);

	Class* super = FindClass(superNum);
	if (!super)
		Fatal("Can't find superclass for %s\n", sym->name);
	Class* theClass = New Class(super);
	sym->obj = theClass;
	theClass->super = superNum;
	theClass->script = scriptNum;
	theClass->num = classNum;
	theClass->sym = sym;
	theClass->file = superFile;
	if (classNum > maxClassNum)
		maxClassNum = classNum;
	if (classNum >= 0 && classes[classNum] == 0)
		classes[classNum] = theClass;
	else {
		Severe("%s is already class #%d.",classes[classNum]->sym->name, classNum);
		return;
	}

	// Get properties and methods.
	for (GetToken(); OpenP(symType); GetToken()) {
		GetToken();
		switch (Keyword()) {
			case K_PROPLIST:
				DefClassItems(theClass, T_PROP);
				break;

			case K_METHODLIST:
				DefClassItems(theClass, T_METHOD);
				break;

			default:
				Severe("Only properties or methods allowed in 'class': %s", symStr);
		}
		CloseBlock();
	}

	UnGetTok();
}

void
DefClassItems(
	Class*	theClass,
	int		what)
{
	// Handle property/method definitions for this class.
	//
	// _property-list ::=	'properties' (symbol [number])+
	// _method-list ::=		'methods' symbol+

	for (Symbol *sym = LookupTok(); !CloseP(symType); sym = LookupTok()) {
		// Make sure the symbol has been defined as a selector.
		if (!sym || symType != S_SELECT) {
			Error("Not a selector: %s", symStr);
			if (PropTag(what)) {
				// Eat the property initialization value.
				GetToken();
				if (!IsNumber())
					UnGetTok();
			}
			continue;
		}

		// If the selector is already a selector of a different sort, complain.
		Selector* tn = theClass->findSelector(sym);
		if (tn && PropTag(what) != IsProperty(tn)) {
			Error("Already defined as %s: %s",
					IsProperty(tn)? "property" : "method", symStr);
			if (PropTag(what)) {
				GetToken();
				if (!IsNumber())
					UnGetTok();
			}
			continue;
		}

		// Install selector.
		if (!tn)
			tn = theClass->addSelector(sym, what);
		if (!PropTag(what))
			tn->tag = T_LOCAL;
		else {
			switch (symVal) {
				case SEL_METHDICT:
					tn->tag = T_METHDICT;
					GetNumber("initial selector value");
					break;
				case SEL_PROPDICT:
					tn->tag = T_PROPDICT;
					GetNumber("initial selector value");
					break;
				default:
					tn->tag = T_PROP;
					GetNumber("initial selector value");
					break;
				}
			tn->val = symVal;
		}
	}

	UnGetTok();
}

int
GetClassNumber(
	Class* theClass)
{
	// Return the first free class number.

	for (int i = 0; i < MAXCLASSES; ++i)
		if (classes[i] == NULL) {
			classes[i] = theClass;
			if (i > maxClassNum)
				maxClassNum = i;
			return i;
		}

	Fatal("Hey! Out of class numbers!!! (Max is %d).", MAXCLASSES);

	// Never reached.
	return 0;
}

Class*
FindClass(int n)
{
	for (Symbol* sp = syms.classSymTbl->firstSym();
		  sp;
		  sp = syms.classSymTbl->nextSym())
		if (sp->obj && sp->obj->num == n)
			return (Class*) sp->obj;

	return 0;
}

Class*
NextClass(
	int n)
{
	// Return a pointer to the class whose class number is the next after n.

	Symbol*	sp;
	Object*	cp;
	int		m;

	cp = 0;
	m = 0x7fff;
	for (sp = syms.classSymTbl->firstSym();
		  sp;
		  sp = syms.classSymTbl->nextSym())
		if (sp->obj->num > n && sp->obj->num < m) {
			cp = sp->obj;
			m = cp->num;
		}

	return (Class*) cp;
}

///////////////////////////////////////////////////////////////////////////

Class::Class() :

	subClasses(0), nextSibling(0)
{
}

Class::Class(
	Class* theSuper) :

	Object(theSuper), subClasses(0), nextSibling(0)
{
	//	add this class to the end of the super's children

	Class** p;
	for (p = &theSuper->subClasses; *p; p = &(*p)->nextSibling)
		;
	*p = this;
}

Bool
Class::selectorDiffers(
	Selector* tp)
{
	// Return true if either the selector referred to by 'tp' is not in
	// this class or if its value differs.

	Selector* stp;

	if (num == -1)
		return True;

	stp = findSelector(tp->sym);
	return !stp ||
			 IsMethod(tp) && tp->tag == T_LOCAL ||
			 tp->tag == T_PROP && tp->val != stp->val;
}

