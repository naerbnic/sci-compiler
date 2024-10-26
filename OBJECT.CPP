//	object.cpp
// 	handle class code/instances.

#include <setjmp.h>

#include "sol.hpp"

#include "string.hpp"

#include	"sc.hpp"

#include	"compile.hpp"
#include	"define.hpp"
#include	"error.hpp"
#include	"input.hpp"
#include	"object.hpp"
#include	"parse.hpp"
#include	"symtbl.hpp"
#include	"text.hpp"
#include	"token.hpp"
#include	"update.hpp"

Object*	curObj;
Object*	receiver;
Symbol*	nameSymbol;
Bool		noAutoName;

static void	Declaration(Object*, int);
static void	InstanceBody(Object*);
static void	MethodDef(Object*);

Object::Object() :

	sym(0), num(0), super(0), script(0), selectors(0), selTail(0),
	numProps(0), an(0), file(0)

#if defined(PLAYGRAMMER)
	,fullFileName(0), srcStart(0), srcEnd(0)
#endif
{
}

Object::Object(
	Class* theSuper) :

	sym(0), num(0), super(0), script(0), selectors(0), selTail(0),
	numProps(0), an(0), file(0)
#if defined(PLAYGRAMMER)
	,fullFileName(0), srcStart(0), srcEnd(0)
#endif
{
	super = theSuper->num;
	dupSelectors(theSuper);
}

Object::~Object()
{
	freeSelectors();

	delete[] file;

#if defined(PLAYGRAMMER)
	delete[] fullFileName;
#endif

	sym->obj = 0;
}

///////////////////////////////////////////////////////////////////////////

void
DoClass()
{
	// class ::= 'class' class-name instance-body

	Class* theClass;

	// Since we're defining a class, we'll need to rewrite the classdef file.
	classAdded = True;

	int classNum = OBJECTNUM;
	int superNum = OBJECTNUM;

	Symbol* sym = LookupTok();

	if (!sym)
		sym = syms.installClass(symStr);

	else if (symType != S_CLASS && symType != S_OBJ) {
		Severe("Redefinition of %s.", symStr);
		return;

	} else {
		theClass = (Class*) sym->obj;

		//	the class is being redefined
		if (theClass) {
			classNum = theClass->num;
			superNum = theClass->super;

			theClass->freeSelectors();

			//	free its filenames
			delete[] theClass->file;
#if defined(PLAYGRAMMER)
			delete[] theClass->fullFileName;
#endif
		}

		//	make sure the symbol is in the class symbol table
		if (sym->type != S_CLASS) {
			syms.remove(sym->name);
			sym->type = S_CLASS;
			syms.classSymTbl->add(sym);
		}
	}

	// Get and verify the 'kindof' keyword.
	GetKeyword(K_OF);

	// Get the super-class and create this class as an instance of it.
	Symbol *superSym = LookupTok();
	if (!superSym || symType != S_CLASS) {
		Severe("%s is not a class.", symStr);
		return;
	}

	Class* super = (Class*) superSym->obj;
	if (superNum != OBJECTNUM && superNum != super->num)
		Fatal("Can't change superclass of %s", sym->name);

	if (superNum != OBJECTNUM)
		theClass->dupSelectors(super);

	else {
		theClass = New Class(super);
		theClass->num = classNum =
			classNum == OBJECTNUM ? GetClassNumber(theClass) : classNum;
		theClass->sym = sym;
		sym->obj = theClass;
		classes[classNum] = theClass;
	}

	// Set the super-class number for this class.
	Selector* sn = theClass->findSelector("-super-");
	if (sn)
		sn->val = super->num;

	theClass->script = script;
	theClass->file = newStr(theFile->fileName);

	// Get any properties, methods, or procedures for this class.
	InstanceBody(theClass);

#if defined(PLAYGRAMMER)
	theClass->fullFileName	= newStr(theFile->fullFileName);
	theClass->srcStart		= GetParseStart();
	theClass->srcEnd			= GetTokenEnd();
#endif
}

void
Instance()
{
	// Define an object as an instance of a class.
	// instance ::=	'instance' symbol 'of' class-name instance-body

	// Get the symbol for the object.
	Symbol* objSym = LookupTok();
	if (!objSym)
		objSym = syms.installLocal(symStr, S_OBJ);
	else if (symType == S_IDENT || symType == S_OBJ) {
		objSym->type = symType = S_OBJ;
		if (objSym->obj)
			Error("Duplicate instance name: %s", objSym->name);
	} else {
		Severe("Redefinition of %s.", symStr);
		return;
	}

	// Get the 'of' keyword.
	GetKeyword(K_OF);

	// Get the class of which this object is an instance.
	Symbol* sym = LookupTok();
	if (!sym || sym->type != S_CLASS) {
		Severe("%s is not a class.", symStr);
		return;
	}
	Class* super = (Class*) sym->obj;

	// Create the object as an instance of the class.
	Object* obj = New Object(super);
	obj->num = OBJECTNUM;
	obj->sym = objSym;
	objSym->obj = obj;

	// Set the super-class number for this object.
	Selector *sn = obj->findSelector("-super-");
	if (sn)
		sn->val = super->num;

	// Get any properties or methods for this object.
	InstanceBody(obj);
}

static void
InstanceBody(
	Object*	obj)
{
	// instance-body ::= (property-list | method-def | procedure)*

	SymTbl* symTbl = syms.add(ST_MINI);

	jmp_buf savedBuf;
	memcpy(savedBuf, recoverBuf, sizeof(jmp_buf));

	// Get a pointer to the 'name' selector for this object and zero
	// out the property.
	Selector* nameSelector = obj->findSelector(nameSymbol);
	if (nameSelector)
		nameSelector->val = -1;

	// Get any property or method definitions.
	curObj = obj;
	for (GetToken() ; OpenP(symType) ; GetToken()) {
		setjmp(recoverBuf);
		GetToken();
		switch (Keyword()) {
			case K_PROPLIST:
				Declaration(obj, T_PROP);
				break;

			case K_METHODLIST:
				Declaration(obj, T_METHOD);
				break;

			case K_METHOD:
				MethodDef(obj);
				break;

			case K_PROC:
				Procedure();
				break;

			case K_DEFINE:
				Define();
				break;

			case K_ENUM:
				Enum();
				break;

			case K_CLASS:
			case K_INSTANCE:
				// Oops!  Got out of synch!
				Error("Mismatched parentheses!");
				memcpy(recoverBuf, savedBuf, sizeof(jmp_buf));
				longjmp(recoverBuf, 1);

			default:
				Severe("Only property and method definitions allowed: %s.", symStr);
				break;
		}

		CloseBlock();
	}

	UnGetTok();

	memcpy(recoverBuf, savedBuf, sizeof(jmp_buf));

	// If 'name' has not been given a value, give it the
	// name of the symbol.
	if (!noAutoName && nameSelector && nameSelector->val == -1) {
		nameSelector->tag = T_TEXT;
		nameSelector->val = text.find(obj->sym->name);
	}

	// The CLASSBIT of the '-info-' property is set for a class.  If this
	// is an instance, clear this bit.
	Selector* sn = obj->findSelector("-info-");
	if (sn && obj->sym->type == S_OBJ)
		sn->val &= ~CLASSBIT;

	// Set the number of properties for this object.
	sn = obj->findSelector("-size-");
	if (sn)
		sn->val = obj->numProps;

	// Set the class number of this object. (The class number is stored
	// temporarily in the '-script-' property, then overwritten when
	// loaded by the interpreter.)
	sn = obj->findSelector("-script-");
	if (sn)
		sn->val = obj->num;

	// Compile code for the object.
	MakeObject(obj);
	curObj = 0;

	syms.deactivate(symTbl);
}

static void
Declaration(
	Object*	obj,
	int		type)
{
	char		msg[80];

	for (GetToken(); !CloseP(symType); GetToken()) {
		if (OpenP(symType)) {
			Definition();
			continue;
		}

		Symbol* sym = syms.lookup(symStr);
		if (!sym && obj->num != OBJECTNUM) {
			// If the symbol is not currently defined, define it as
			// the next selector number.
			InstallSelector(symStr, NewSelectorNum());
			sym = syms.lookup(symStr);
		}

		// If this selector is not in the current class, add it.
		Selector* sn = sym ? obj->findSelector(sym) : 0;
		if (!sn) {
			if (obj->num != OBJECTNUM)
				sn = ((Class*) obj)->addSelector(sym, type);
			else {
				// Can't define New properties or methods in an instance.
				Error("Can't declare property or method in instance.");
				GetToken();
				if (!IsNumber())
					UnGetTok();
				continue;
			}
		}

		if (sym->type != S_SELECT ||
			 type == T_PROP && !IsProperty(sn) ||
			 type == T_METHOD && IsProperty(sn)) {
			Error("Not a %s: %s.", type == T_PROP ? "property" : "method", symStr);
			GetToken();
			if (IsNumber())
				UnGetTok();
			continue;
		}

		if (type == T_PROP) {
			GetNumberOrString(msg);
			sn->val = symVal;
			switch (symType) {
				case S_NUM:
					sn->tag = T_PROP;
					break;
				case S_STRING:
					sn->tag = T_TEXT;
					break;
			}
		}
	}

	UnGetTok();
}

void
MethodDef(
	Object* obj)
{
	// _method-def ::= 'method' call-def expression*

	SymTbl*	symTbl = syms.add(ST_MINI);

	PNode*	node = CallDef(S_SELECT);
	if (node) {
		Symbol* sym = node->sym;

		Selector* sn = obj->findSelector(sym);
		if (sym->type != S_SELECT || IsProperty(sn))
			Error("Not a method: %s", sym->name);
		else if (sym->an)
			Error("Method already defined: %s", sym->name);
		else {
			// Compile the code for this method.
			ExprList(node, OPTIONAL);
			CompileCode(node);

			// Save the pointer to the method code.
			sn->tag = T_LOCAL;
			sn->an = sym->an;
		}
	}

	syms.deactivate(symTbl);
}
