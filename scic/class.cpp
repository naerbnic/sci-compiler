//	class.cpp
// 	code to deal with classes.

#include "scic/class.hpp"

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>

#include "scic/error.hpp"
#include "scic/object.hpp"
#include "scic/parse.hpp"
#include "scic/symtbl.hpp"
#include "scic/token.hpp"
#include "scic/toktypes.hpp"

#define MAXCLASSES 512  // Maximum number of classes

Class* gClasses[MAXCLASSES];
int gMaxClassNum = -1;

static void DefClassItems(Class* theClass, int what);

void InstallObjects() {
  // Install 'RootObj' as the root of the class system.
  Symbol* sym = gSyms.installClass("RootObj");
  auto rootClassOwned = std::make_unique<Class>();
  auto* rootClass = rootClassOwned.get();
  sym->setObj(std::move(rootClassOwned));
  rootClass->sym = sym;
  rootClass->script = KERNEL;
  rootClass->num = -1;

  // Install the root class' selectors in the symbol table and add them
  // to the root class.
  InstallSelector("-objID-", SEL_OBJID);
  if ((sym = gSyms.lookup("-objID-")))
    rootClass->addSelector(sym, T_PROP)->val = 0x1234;

  InstallSelector("-size-", SEL_SIZE);
  if ((sym = gSyms.lookup("-size-"))) rootClass->addSelector(sym, T_PROP);

  InstallSelector("-propDict-", SEL_PROPDICT);
  if ((sym = gSyms.lookup("-propDict-"))) {
    rootClass->addSelector(sym, T_PROPDICT);
  }

  InstallSelector("-methDict-", SEL_METHDICT);
  if ((sym = gSyms.lookup("-methDict-")))
    rootClass->addSelector(sym, T_METHDICT);

  InstallSelector("-classScript-", SEL_CLASS_SCRIPT);
  if ((sym = gSyms.lookup("-classScript-")))
    rootClass->addSelector(sym, T_PROP)->val = 0;

  InstallSelector("-script-", SEL_SCRIPT);
  if ((sym = gSyms.lookup("-script-"))) rootClass->addSelector(sym, T_PROP);

  InstallSelector("-super-", SEL_SUPER);
  if ((sym = gSyms.lookup("-super-")))
    rootClass->addSelector(sym, T_PROP)->val = -1;

  InstallSelector("-info-", SEL_INFO);
  if ((sym = gSyms.lookup("-info-")))
    rootClass->addSelector(sym, T_PROP)->val = CLASSBIT;

  // Install 'self' and 'super' as objects.
  sym = gSyms.installGlobal("self", S_OBJ);
  sym->setVal((int)OBJ_SELF);
  sym = gSyms.installGlobal("super", S_CLASS);
  sym->setVal((int)OBJ_SUPER);
}

void DefineClass() {
  // class-def ::=	'classdef' symbol 'kindof' ('RootObj' | class-name)
  //				'script#' number 'class#' number 'super#' number
  //'file#' string 				(property-list | method-list)*

  // Get and install the name of this class.  The class is installed in
  // the class symbol table, which will be used to write out classdefs
  // later.
  Symbol* sym = LookupTok();
  if (!sym)
    sym = gSyms.installClass(gSymStr);

  else if (symType() == S_IDENT || symType() == S_OBJ) {
    gSyms.del(gSymStr);
    sym = gSyms.installClass(gSymStr);

  } else {
    Severe("Redefinition of %s.", gSymStr);
    return;
  }

  // Get the script, class, super-class numbers, and file name.
  GetKeyword(K_SCRIPTNUM);
  GetNumber("Script #");
  int scriptNum = symVal();
  GetKeyword(K_CLASSNUM);
  GetNumber("Class #");
  int classNum = symVal();
  GetKeyword(K_SUPER);
  GetNumber("Super #");
  int superNum = symVal();
  GetKeyword(K_FILE);
  GetString("File name");
  std::string_view superFile = gSymStr;

  Class* super = FindClass(superNum);
  if (!super) Fatal("Can't find superclass for %s\n", sym->name());
  auto theClassOwned = std::make_unique<Class>(super);
  auto* theClass = theClassOwned.get();
  sym->setObj(std::move(theClassOwned));
  theClass->super = superNum;
  theClass->script = scriptNum;
  theClass->num = classNum;
  theClass->sym = sym;
  theClass->file = superFile;
  if (classNum > gMaxClassNum) gMaxClassNum = classNum;
  if (classNum >= 0 && gClasses[classNum] == 0)
    gClasses[classNum] = theClass;
  else {
    Severe("%s is already class #%d.", gClasses[classNum]->sym->name(),
           classNum);
    return;
  }

  // Get properties and methods.
  for (GetToken(); OpenP(symType()); GetToken()) {
    GetToken();
    switch (Keyword()) {
      case K_PROPLIST:
        DefClassItems(theClass, T_PROP);
        break;

      case K_METHODLIST:
        DefClassItems(theClass, T_METHOD);
        break;

      default:
        Severe("Only properties or methods allowed in 'class': %s", gSymStr);
    }
    CloseBlock();
  }

  UnGetTok();
}

void DefClassItems(Class* theClass, int what) {
  // Handle property/method definitions for this class.
  //
  // _property-list ::=	'properties' (symbol [number])+
  // _method-list ::=		'methods' symbol+

  for (Symbol* sym = LookupTok(); !CloseP(symType()); sym = LookupTok()) {
    // Make sure the symbol has been defined as a selector.
    if (!sym || symType() != S_SELECT) {
      Error("Not a selector: %s", gSymStr);
      if (PropTag(what)) {
        // Eat the property initialization value.
        GetToken();
        if (!IsNumber()) UnGetTok();
      }
      continue;
    }

    // If the selector is already a selector of a different sort, complain.
    Selector* tn = theClass->findSelectorByNum(sym->val());
    if (tn && PropTag(what) != IsProperty(tn)) {
      Error("Already defined as %s: %s", IsProperty(tn) ? "property" : "method",
            gSymStr);
      if (PropTag(what)) {
        GetToken();
        if (!IsNumber()) UnGetTok();
      }
      continue;
    }

    // Install selector.
    if (!tn) tn = theClass->addSelector(sym, what);
    if (!PropTag(what))
      tn->tag = T_LOCAL;
    else {
      switch (symVal()) {
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
      tn->val = symVal();
    }
  }

  UnGetTok();
}

int GetClassNumber(Class* theClass) {
  // Return the first free class number.

  for (int i = 0; i < MAXCLASSES; ++i)
    if (gClasses[i] == NULL) {
      gClasses[i] = theClass;
      if (i > gMaxClassNum) gMaxClassNum = i;
      return i;
    }

  Fatal("Hey! Out of class numbers!!! (Max is %d).", MAXCLASSES);

  // Never reached.
  return 0;
}

Class* FindClass(int n) {
  for (auto* sp : gSyms.classSymTbl->symbols())
    if (sp->obj() && sp->obj()->num == n) return (Class*)sp->obj();

  return 0;
}

Class* NextClass(int n) {
  // Return a pointer to the class whose class number is the next after n.

  Object* cp;
  int m;

  cp = 0;
  m = 0x7fff;
  for (auto* sp : gSyms.classSymTbl->symbols())
    if (sp->obj()->num > n && sp->obj()->num < m) {
      cp = sp->obj();
      m = cp->num;
    }

  return (Class*)cp;
}

///////////////////////////////////////////////////////////////////////////

Class::Class()
    :

      subClasses(0),
      nextSibling(0) {}

Class::Class(Class* theSuper)
    :

      Object(theSuper),
      subClasses(0),
      nextSibling(0) {
  //	add this class to the end of the super's children

  Class** p;
  for (p = &theSuper->subClasses; *p; p = &(*p)->nextSibling);
  *p = this;
}

bool Class::selectorDiffers(Selector* tp) {
  // Return true if either the selector referred to by 'tp' is not in
  // this class or if its value differs.

  Selector* stp;

  if (num == -1) return true;

  stp = findSelectorByNum(tp->sym->val());
  return !stp || (IsMethod(tp) && tp->tag == T_LOCAL) ||
         (tp->tag == T_PROP && tp->val != stp->val);
}

Selector* Class::addSelector(Symbol* sym, int what) {
  // Add a selector ('sym') to the selectors for this class.
  // Allocate a selector node, and link it into the selector list for
  // the class.  Finally, return a pointer to the selector node.

  if (!sym) return 0;

  auto sn_owned = std::make_unique<Selector>(sym);
  auto* sn = sn_owned.get();
  selectors_.push_back(std::move(sn_owned));

  switch (sym->val()) {
    case SEL_METHDICT:
      sn->tag = T_METHDICT;
      break;
    case SEL_PROPDICT:
      sn->tag = T_PROPDICT;
      break;
    default:
      sn->tag = (uint8_t)what;
      break;
  }

  if (PropTag(what)) {
    sn->ofs = 2 * numProps;
    ++numProps;
  }

  return sn;
}
