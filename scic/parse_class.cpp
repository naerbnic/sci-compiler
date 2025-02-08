#include "scic/parse_class.hpp"

#include <fcntl.h>
#include <sys/stat.h>

#include <memory>
#include <string_view>
#include <utility>

#include "scic/class.hpp"
#include "scic/error.hpp"
#include "scic/object.hpp"
#include "scic/parse.hpp"
#include "scic/parse_context.hpp"
#include "scic/selector.hpp"
#include "scic/symbol.hpp"
#include "scic/symtbl.hpp"
#include "scic/symtypes.hpp"
#include "scic/token.hpp"
#include "scic/toktypes.hpp"

static void DefClassItems(Class* theClass, int what);

void InstallObjects(ParseContext* parseContext) {
  // Install 'RootObj' as the root of the class system.
  Symbol* sym = parseContext->syms.installClass("RootObj");
  auto rootClassOwned = std::make_unique<Class>();
  auto* rootClass = rootClassOwned.get();
  sym->setObj(std::move(rootClassOwned));
  rootClass->sym = sym;
  rootClass->name = "RootObj";
  rootClass->script = KERNEL;
  rootClass->num = -1;

  // Install the root class' selectors in the symbol table and add them
  // to the root class.
  InstallSelector("-objID-", SEL_OBJID);
  if ((sym = parseContext->syms.lookup("-objID-")))
    rootClass->addSelector(sym, T_PROP)->val = 0x1234;

  InstallSelector("-size-", SEL_SIZE);
  if ((sym = parseContext->syms.lookup("-size-")))
    rootClass->addSelector(sym, T_PROP);

  InstallSelector("-propDict-", SEL_PROPDICT);
  if ((sym = parseContext->syms.lookup("-propDict-"))) {
    rootClass->addSelector(sym, T_PROPDICT);
  }

  InstallSelector("-methDict-", SEL_METHDICT);
  if ((sym = parseContext->syms.lookup("-methDict-")))
    rootClass->addSelector(sym, T_METHDICT);

  InstallSelector("-classScript-", SEL_CLASS_SCRIPT);
  if ((sym = parseContext->syms.lookup("-classScript-")))
    rootClass->addSelector(sym, T_PROP)->val = 0;

  InstallSelector("-script-", SEL_SCRIPT);
  if ((sym = parseContext->syms.lookup("-script-")))
    rootClass->addSelector(sym, T_PROP);

  InstallSelector("-super-", SEL_SUPER);
  if ((sym = parseContext->syms.lookup("-super-")))
    rootClass->addSelector(sym, T_PROP)->val = -1;

  InstallSelector("-info-", SEL_INFO);
  if ((sym = parseContext->syms.lookup("-info-")))
    rootClass->addSelector(sym, T_PROP)->val = CLASSBIT;

  // Install 'self' and 'super' as objects.
  sym = parseContext->syms.installGlobal("self", S_OBJ);
  sym->setVal((int)OBJ_SELF);
  sym = parseContext->syms.installGlobal("super", S_CLASS);
  sym->setVal((int)OBJ_SUPER);
}

void DefineClass() {
  // class-def ::=	'classdef' symbol 'kindof' ('RootObj' | class-name)
  //				'script#' number 'class#' number 'super#' number
  //'file#' string 				(property-list | method-list)*

  // Get and install the name of this class.  The class is installed in
  // the class symbol table, which will be used to write out classdefs
  // later.
  auto slot = LookupTok();
  auto* sym = slot.symbol();
  if (!sym)
    sym = gParseContext.syms.installClass(slot.name());

  else if (slot.type() == S_IDENT || slot.type() == S_OBJ) {
    gParseContext.syms.del(slot.name());
    sym = gParseContext.syms.installClass(slot.name());

  } else {
    Severe("Redefinition of %s.", slot.name());
    return;
  }

  // Get the script, class, super-class numbers, and file name.
  GetKeyword(K_SCRIPTNUM);
  int scriptNum = GetNumber("Script #").value_or(0);
  GetKeyword(K_CLASSNUM);
  int classNum = GetNumber("Class #").value_or(0);
  GetKeyword(K_SUPER);
  int superNum = GetNumber("Super #").value_or(0);
  GetKeyword(K_FILE);
  auto file_name_token = GetString("File name");
  std::string_view superFile = file_name_token ? file_name_token->name() : "";

  Class* super = FindClass(superNum);
  if (!super) Fatal("Can't find superclass for %s\n", sym->name());
  auto theClassOwned = std::make_unique<Class>(super);
  auto* theClass = theClassOwned.get();
  sym->setObj(std::move(theClassOwned));
  theClass->name = sym->name();
  theClass->super = superNum;
  theClass->script = scriptNum;
  theClass->num = classNum;
  theClass->sym = sym;
  theClass->file = superFile;
  if (classNum > gParseContext.maxClassNum)
    gParseContext.maxClassNum = classNum;
  if (classNum >= 0 && gParseContext.classes[classNum] == 0)
    gParseContext.classes[classNum] = theClass;
  else {
    Severe("%s is already class #%d.", gParseContext.classes[classNum]->name,
           classNum);
    return;
  }

  // Get properties and methods.
  for (auto outer_token = GetToken(); OpenP(outer_token.type());
       outer_token = GetToken()) {
    auto token = GetToken();
    switch (Keyword(token)) {
      case K_PROPLIST:
        DefClassItems(theClass, T_PROP);
        break;

      case K_METHODLIST:
        DefClassItems(theClass, T_METHOD);
        break;

      default:
        Severe("Only properties or methods allowed in 'class': %s",
               token.name());
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

  for (ResolvedTokenSlot slot = LookupTok(); !CloseP(slot.type());
       slot = LookupTok()) {
    // Make sure the symbol has been defined as a selector.
    if (!slot.is_resolved() || slot.type() != S_SELECT) {
      Error("Not a selector: %s", slot.name());
      if (PropTag(what)) {
        // Eat the property initialization value.
        auto init_token = GetToken();
        if (!IsNumber(init_token)) UnGetTok();
      }
      continue;
    }

    // If the selector is already a selector of a different sort, complain.
    Selector* tn = theClass->findSelectorByNum(slot.val());
    if (tn && PropTag(what) != IsProperty(tn)) {
      Error("Already defined as %s: %s", IsProperty(tn) ? "property" : "method",
            slot.name());
      if (PropTag(what)) {
        auto value = GetToken();
        if (!IsNumber(value)) UnGetTok();
      }
      continue;
    }

    // Install selector.
    if (!tn) tn = theClass->addSelector(slot.symbol(), what);
    if (!PropTag(what))
      tn->tag = T_LOCAL;
    else {
      switch (slot.val()) {
        case SEL_METHDICT:
          tn->tag = T_METHDICT;
          tn->val = GetNumber("initial selector value").value_or(0);
          break;
        case SEL_PROPDICT:
          tn->tag = T_PROPDICT;
          tn->val = GetNumber("initial selector value").value_or(0);
          break;
        default:
          tn->tag = T_PROP;
          tn->val = GetNumber("initial selector value").value_or(0);
          break;
      }
    }
  }

  UnGetTok();
}

int GetClassNumber(Class* theClass) {
  // Return the first free class number.

  for (int i = 0; i < MAXCLASSES; ++i)
    if (gParseContext.classes[i] == nullptr) {
      gParseContext.classes[i] = theClass;
      if (i > gParseContext.maxClassNum) gParseContext.maxClassNum = i;
      return i;
    }

  Fatal("Hey! Out of class numbers!!! (Max is %d).", MAXCLASSES);

  // Never reached.
  return 0;
}

Class* FindClass(int n) {
  for (auto* sp : gParseContext.syms.classSymTbl->symbols())
    if (sp->obj() && sp->obj()->num == n) return (Class*)sp->obj();

  return 0;
}

Class* NextClass(int n) {
  // Return a pointer to the class whose class number is the next after n.

  Object* cp;
  int m;

  cp = 0;
  m = 0x7fff;
  for (auto* sp : gParseContext.syms.classSymTbl->symbols())
    if (sp->obj()->num > n && sp->obj()->num < m) {
      cp = sp->obj();
      m = cp->num;
    }

  return (Class*)cp;
}

///////////////////////////////////////////////////////////////////////////