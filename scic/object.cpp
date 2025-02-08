//	object.cpp
// 	handle class code/instances.

#include "scic/object.hpp"

#include <csetjmp>
#include <cstring>
#include <memory>
#include <string_view>
#include <utility>

#include "scic/class.hpp"
#include "scic/compile.hpp"
#include "scic/config.hpp"
#include "scic/define.hpp"
#include "scic/error.hpp"
#include "scic/expr.hpp"
#include "scic/input.hpp"
#include "scic/parse.hpp"
#include "scic/proc.hpp"
#include "scic/sc.hpp"
#include "scic/selector.hpp"
#include "scic/symbol.hpp"
#include "scic/symtbl.hpp"
#include "scic/symtypes.hpp"
#include "scic/text.hpp"
#include "scic/token.hpp"
#include "scic/toktypes.hpp"
#include "scic/update.hpp"

Object* gCurObj;
Object* gReceiver;
Symbol* gNameSymbol;

static void Declaration(Object*, int);
static void InstanceBody(Object*);
static void MethodDef(Object*);

Object::Object() : sym(0), num(0), super(0), script(0), numProps(0), an(0) {}

Object::Object(Class* theSuper)
    : sym(0), num(0), super(0), script(0), numProps(0), an(0) {
  super = theSuper->num;
  dupSelectors(theSuper);
}

void Object::dupSelectors(Class* super) {
  // duplicate super's selectors

  for (auto* sn : super->selectors()) {
    auto tn = std::make_unique<Selector>();
    *tn = *sn;
    if (tn->tag == T_LOCAL) {
      tn->tag = T_METHOD;  // No longer a local method.
      tn->an = nullptr;    // No code defined for this class.
    }
    selectors_.push_back(std::move(tn));
  }

  numProps = super->numProps;
}

Selector* Object::findSelectorByNum(int val) {
  // Return a pointer to the selector node which corresponds to the
  //	symbol 'sym'.

  for (auto* sn : selectors()) {
    if (val == sn->sym->val()) return sn;
  }

  return nullptr;
}

Selector* Object::findSelector(std::string_view name) {
  // Return a pointer to the selector node which has the name 'name'.

  Symbol* sym = gSyms.lookup(name);
  return sym ? findSelectorByNum(sym->val()) : 0;
}

void Object::freeSelectors() {
  // free the object's selectors

  selectors_.clear();
}

///////////////////////////////////////////////////////////////////////////

void DoClass() {
  // class ::= 'class' class-name instance-body

  Class* theClass;

  // Since we're defining a class, we'll need to rewrite the classdef file.
  gClassAdded = true;

  int classNum = OBJECTNUM;
  int superNum = OBJECTNUM;

  Symbol* sym = LookupTok();

  if (!sym)
    sym = gSyms.installClass(gSymStr);

  else if (symType() != S_CLASS && symType() != S_OBJ) {
    Severe("Redefinition of %s.", gSymStr);
    return;

  } else {
    theClass = (Class*)sym->obj();

    //	the class is being redefined
    if (theClass) {
      classNum = theClass->num;
      superNum = theClass->super;

      theClass->freeSelectors();

      //	free its filenames
      theClass->file.clear();
    }

    //	make sure the symbol is in the class symbol table
    if (sym->type != S_CLASS) {
      auto symOwned = gSyms.remove(sym->name());
      symOwned->type = S_CLASS;
      gSyms.classSymTbl->add(std::move(symOwned));
    }
  }

  // Get and verify the 'kindof' keyword.
  GetKeyword(K_OF);

  // Get the super-class and create this class as an instance of it.
  Symbol* superSym = LookupTok();
  if (!superSym || symType() != S_CLASS) {
    Severe("%s is not a class.", gSymStr);
    return;
  }

  Class* super = (Class*)superSym->obj();
  if (superNum != OBJECTNUM && superNum != super->num)
    Fatal("Can't change superclass of %s", sym->name());

  if (superNum != OBJECTNUM)
    theClass->dupSelectors(super);

  else {
    auto theClassOwned = std::make_unique<Class>(super);
    theClass = theClassOwned.get();
    theClass->num = classNum =
        classNum == OBJECTNUM ? GetClassNumber(theClass) : classNum;
    theClass->sym = sym;
    sym->setObj(std::move(theClassOwned));
    gClasses[classNum] = theClass;
  }

  // Set the super-class number for this class.
  Selector* sn = theClass->findSelector("-super-");
  if (sn) sn->val = super->num;

  theClass->script = gScript;
  theClass->file = gInputState.GetCurrFileName();

  // Get any properties, methods, or procedures for this class.
  InstanceBody(theClass);
}

void Instance() {
  // Define an object as an instance of a class.
  // instance ::=	'instance' symbol 'of' class-name instance-body

  // Get the symbol for the object.
  Symbol* objSym = LookupTok();
  if (!objSym)
    objSym = gSyms.installLocal(gSymStr, S_OBJ);
  else if (symType() == S_IDENT || symType() == S_OBJ) {
    objSym->type = S_OBJ;
    setSymType(S_OBJ);
    if (objSym->obj()) Error("Duplicate instance name: %s", objSym->name());
  } else {
    Severe("Redefinition of %s.", gSymStr);
    return;
  }

  // Get the 'of' keyword.
  GetKeyword(K_OF);

  // Get the class of which this object is an instance.
  Symbol* sym = LookupTok();
  if (!sym || sym->type != S_CLASS) {
    Severe("%s is not a class.", gSymStr);
    return;
  }
  Class* super = (Class*)sym->obj();

  // Create the object as an instance of the class.
  auto objOwned = std::make_unique<Object>(super);
  auto* obj = objOwned.get();
  obj->num = OBJECTNUM;
  obj->sym = objSym;
  objSym->setObj(std::move(objOwned));

  // Set the super-class number for this object.
  Selector* sn = obj->findSelector("-super-");
  if (sn) sn->val = super->num;

  // Get any properties or methods for this object.
  InstanceBody(obj);
}

static void InstanceBody(Object* obj) {
  // instance-body ::= (property-list | method-def | procedure)*

  SymTbl* symTbl = gSyms.add(ST_MINI);

  jmp_buf savedBuf;
  memcpy(savedBuf, gRecoverBuf, sizeof(jmp_buf));

  // Get a pointer to the 'name' selector for this object and zero
  // out the property.
  Selector* nameSelector = obj->findSelectorByNum(gNameSymbol->val());
  if (nameSelector) nameSelector->val = -1;

  // Get any property or method definitions.
  gCurObj = obj;
  for (GetToken(); OpenP(symType()); GetToken()) {
    // The original code ignores the return value of setjmp.
    (void)setjmp(gRecoverBuf);
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
        memcpy(gRecoverBuf, savedBuf, sizeof(jmp_buf));
        longjmp(gRecoverBuf, 1);

      default:
        Severe("Only property and method definitions allowed: %s.", gSymStr);
        break;
    }

    CloseBlock();
  }

  UnGetTok();

  memcpy(gRecoverBuf, savedBuf, sizeof(jmp_buf));

  // If 'name' has not been given a value, give it the
  // name of the symbol.
  if (!gConfig->noAutoName && nameSelector && nameSelector->val == -1) {
    nameSelector->tag = T_TEXT;
    nameSelector->val = gText.find(obj->sym->name());
  }

  // The CLASSBIT of the '-info-' property is set for a class.  If this
  // is an instance, clear this bit.
  Selector* sn = obj->findSelector("-info-");
  if (sn && obj->sym->type == S_OBJ) sn->val &= ~CLASSBIT;

  // Set the number of properties for this object.
  sn = obj->findSelector("-size-");
  if (sn) sn->val = obj->numProps;

  // Set the class number of this object. (The class number is stored
  // temporarily in the '-script-' property, then overwritten when
  // loaded by the interpreter.)
  sn = obj->findSelector("-script-");
  if (sn) sn->val = obj->num;

  // Compile code for the object.
  MakeObject(obj);
  gCurObj = 0;

  gSyms.deactivate(symTbl);
}

static void Declaration(Object* obj, int type) {
  char msg[80];

  for (GetToken(); !CloseP(symType()); GetToken()) {
    if (OpenP(symType())) {
      Definition();
      continue;
    }

    Symbol* sym = gSyms.lookup(gSymStr);
    if (!sym && obj->num != OBJECTNUM) {
      // If the symbol is not currently defined, define it as
      // the next selector number.
      InstallSelector(gSymStr, NewSelectorNum());
      sym = gSyms.lookup(gSymStr);
    }

    // If this selector is not in the current class, add it.
    Selector* sn = sym ? obj->findSelectorByNum(sym->val()) : 0;
    if (!sn) {
      if (obj->num != OBJECTNUM)
        sn = ((Class*)obj)->addSelector(sym, type);
      else {
        // Can't define new properties or methods in an instance.
        Error("Can't declare property or method in instance.");
        GetToken();
        if (!IsNumber()) UnGetTok();
        continue;
      }
    }

    if (sym->type != S_SELECT || (type == T_PROP && !IsProperty(sn)) ||
        (type == T_METHOD && IsProperty(sn))) {
      Error("Not a %s: %s.", type == T_PROP ? "property" : "method", gSymStr);
      GetToken();
      if (IsNumber()) UnGetTok();
      continue;
    }

    if (type == T_PROP) {
      GetNumberOrString(msg);
      sn->val = symVal();
      switch (symType()) {
        case S_NUM:
          sn->tag = T_PROP;
          break;
        case S_STRING:
          sn->tag = T_TEXT;
          break;
        default:
          Fatal("Invalid property type: %s, %d", gSymStr, type);
          break;
      }
    }
  }

  UnGetTok();
}

void MethodDef(Object* obj) {
  // _method-def ::= 'method' call-def expression*

  SymTbl* symTbl = gSyms.add(ST_MINI);
  {
    auto node = CallDef(S_SELECT);
    if (node) {
      Symbol* sym = node->sym;

      Selector* sn = obj->findSelectorByNum(sym->val());
      if (sym->type != S_SELECT || IsProperty(sn))
        Error("Not a method: %s", sym->name());
      else if (sn->an)
        Error("Method already defined: %s", sym->name());
      else {
        // Compile the code for this method.
        ExprList(node.get(), OPTIONAL);
        CompileProc(gSc->hunkList->getList(), node.get());

        // Save the pointer to the method code.
        sn->tag = T_LOCAL;
        sn->an = sym->an();
      }
    }
  }
  gSyms.deactivate(symTbl);
}
