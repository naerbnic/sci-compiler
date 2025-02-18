#include "scic/parse_object.hpp"

#include <csetjmp>
#include <cstring>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>

#include "scic/class.hpp"
#include "scic/codegen/anode.hpp"
#include "scic/codegen/anode_impls.hpp"
#include "scic/codegen/compiler.hpp"
#include "scic/common.hpp"
#include "scic/compile.hpp"
#include "scic/config.hpp"
#include "scic/define.hpp"
#include "scic/error.hpp"
#include "scic/expr.hpp"
#include "scic/global_compiler.hpp"
#include "scic/input.hpp"
#include "scic/object.hpp"
#include "scic/parse.hpp"
#include "scic/parse_class.hpp"
#include "scic/parse_context.hpp"
#include "scic/proc.hpp"
#include "scic/sc.hpp"
#include "scic/selector.hpp"
#include "scic/symbol.hpp"
#include "scic/symtbl.hpp"
#include "scic/symtypes.hpp"
#include "scic/token.hpp"
#include "scic/toktypes.hpp"
#include "scic/update.hpp"
#include "util/types/overload.hpp"

namespace {

void Declaration(Object* obj, int type) {
  char msg[80];

  for (auto token = GetToken(); !CloseP(token.type()); token = GetToken()) {
    if (OpenP(token.type())) {
      Definition();
      continue;
    }

    Symbol* sym = gSyms.lookup(token.name());
    if (!sym && obj->num != OBJECTNUM) {
      // If the symbol is not currently defined, define it as
      // the next selector number.
      InstallSelector(token.name(), NewSelectorNum());
      sym = gSyms.lookup(token.name());
    }

    // If this selector is not in the current class, add it.
    Selector* sn = sym ? obj->findSelectorByNum(sym->val()) : 0;
    if (!sn) {
      if (obj->num != OBJECTNUM)
        sn = ((Class*)obj)->addSelector(sym, type);
      else {
        // Can't define new properties or methods in an instance.
        Error("Can't declare property or method in instance.");
        token = GetToken();
        if (!IsNumber(token)) UnGetTok();
        continue;
      }
    }

    if (sym->type != S_SELECT || (type == T_PROP && !IsProperty(sn)) ||
        (type == T_METHOD && IsProperty(sn))) {
      Error("Not a %s: %s.", type == T_PROP ? "property" : "method",
            token.name());
      token = GetToken();
      if (IsNumber(token)) UnGetTok();
      continue;
    }

    if (type == T_PROP) {
      auto value = GetNumberOrString(msg);
      if (!value) {
        Fatal("Invalid property type: %s, %d", "CHECK THIS VALUE", type);
        continue;
      }
      sn->val = *value;
      std::visit(util::Overload([&](int) { sn->tag = T_PROP; },
                                [&](ANText*) { sn->tag = T_TEXT; }),
                 *value);
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
        CompileProc(node.get());

        // Save the pointer to the method code.
        sn->tag = T_LOCAL;
        sym->forwardRef.RegisterCallback(
            [sn](ANode* target) { sn->an = target; });
      }
    }
  }
  gSyms.deactivate(symTbl);
}

static void InstanceBody(Object* obj) {
  // instance-body ::= (property-list | method-def | procedure)*

  SymTbl* symTbl = gSyms.add(ST_MINI);

  jmp_buf savedBuf;
  memcpy(savedBuf, gRecoverBuf, sizeof(jmp_buf));

  // Get a pointer to the 'name' selector for this object and zero
  // out the property.
  Selector* nameSelector = obj->findSelectorByNum(gNameSymbol->val());
  if (nameSelector) nameSelector->val = std::nullopt;

  // Get any property or method definitions.
  gCurObj = obj;
  for (auto token = GetToken(); OpenP(token.type()); token = GetToken()) {
    // The original code ignores the return value of setjmp.
    (void)setjmp(gRecoverBuf);
    token = GetToken();
    switch (Keyword(token)) {
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
        Severe("Only property and method definitions allowed: %s.",
               token.name());
        break;
    }

    CloseBlock();
  }

  UnGetTok();

  memcpy(gRecoverBuf, savedBuf, sizeof(jmp_buf));

  // If 'name' has not been given a value, give it the
  // name of the symbol.
  if (!gConfig->noAutoName && nameSelector && !nameSelector->val) {
    nameSelector->tag = T_TEXT;
    nameSelector->val = gSc->AddTextNode(obj->name);
  }

  // The CLASSBIT of the '-info-' property is set for a class.  If this
  // is an instance, clear this bit.
  Selector* sn = obj->findSelector("-info-");
  if (sn && !obj->isClass()) std::get<int>(*sn->val) &= ~CLASSBIT;

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

}  // namespace

void DoClass() {
  // class ::= 'class' class-name instance-body

  Class* theClass;

  // Since we're defining a class, we'll need to rewrite the classdef file.
  gClassAdded = true;

  int classNum = OBJECTNUM;
  int superNum = OBJECTNUM;

  auto slot = LookupTok();
  auto* sym = slot.symbol();

  if (!sym)
    sym = gSyms.installClass(slot.name());

  else if (slot.type() != S_CLASS && slot.type() != S_OBJ) {
    Severe("Redefinition of %s.", slot.name());
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
  auto superSlot = LookupTok();
  if (!superSlot.is_resolved() || superSlot.type() != S_CLASS) {
    Severe("%s is not a class.", superSlot.name());
    return;
  }

  Class* super = (Class*)superSlot.symbol()->obj();
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
    theClass->name = sym->name();
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
  auto slot = LookupTok();
  auto* objSym = slot.symbol();
  if (!objSym)
    objSym = gSyms.installLocal(slot.name(), S_OBJ);
  else if (slot.type() == S_IDENT || slot.type() == S_OBJ) {
    objSym->type = S_OBJ;
    if (objSym->obj()) Error("Duplicate instance name: %s", objSym->name());
  } else {
    Severe("Redefinition of %s.", slot.name());
    return;
  }

  // Get the 'of' keyword.
  GetKeyword(K_OF);

  // Get the class of which this object is an instance.
  auto classSlot = LookupTok();
  auto* sym = classSlot.symbol();
  if (!sym || sym->type != S_CLASS) {
    Severe("%s is not a class.", classSlot.name());
    return;
  }
  Class* super = (Class*)sym->obj();

  // Create the object as an instance of the class.
  auto objOwned = std::make_unique<Object>(super);
  auto* obj = objOwned.get();
  obj->num = OBJECTNUM;
  obj->sym = objSym;
  obj->name = objSym->name();
  objSym->setObj(std::move(objOwned));

  // Set the super-class number for this object.
  Selector* sn = obj->findSelector("-super-");
  if (sn) sn->val = super->num;

  // Get any properties or methods for this object.
  InstanceBody(obj);
}