//	expr.cpp		sc
// 	expressions

#include "scic/expr.hpp"

#include <algorithm>
#include <cassert>
#include <csetjmp>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

#include "scic/class.hpp"
#include "scic/define.hpp"
#include "scic/error.hpp"
#include "scic/object.hpp"
#include "scic/parse.hpp"
#include "scic/pnode.hpp"
#include "scic/sc.hpp"
#include "scic/selector.hpp"
#include "scic/symbol.hpp"
#include "scic/symtbl.hpp"
#include "scic/symtypes.hpp"
#include "scic/text.hpp"
#include "scic/token.hpp"
#include "scic/toktypes.hpp"

static int gLoopNest;

static bool _Expression(PNode*);
static bool Return(PNode*);
static bool Call(PNode*, Symbol*);
static bool Send(PNode*, Symbol*);
static bool Message(PNode*, Symbol*);
static bool While(PNode*);
static bool Repeat(PNode*);
static bool For(PNode*);
static bool Break(PNode*);
static bool BreakIf(PNode*);
static bool Continue(PNode*);
static bool ContIf(PNode*);
static bool If(PNode*);
static bool Cond(PNode*);
static bool Switch(PNode*);
static bool SwitchTo(PNode*);
static bool IncDec(PNode*);
static bool Variable(PNode*);
static bool Array(PNode*);
static bool NaryExpr(PNode*);
static bool BinaryExpr(PNode*);
static bool UnaryExpr(PNode*);
static bool CompExpr(PNode*);
static bool Assignment(PNode*);
static bool Rest(PNode*);
static pn_t PNType(sym_t);

bool ExprList(PNode* theNode, bool required) {
  // expression-list ::= expression*

  int numExpr;

  auto pn = std::make_unique<PNode>(PN_ELIST);

  // Get the expressions making up the list.  Even if an expression
  // list is required, only the first expression is required.
  for (numExpr = 0; Expression(pn.get(), required); ++numExpr) required = false;

  // If we successfully got an expression, add it to the parse tree,
  // otherwise delete it.
  if (numExpr) {
    theNode->addChild(std::move(pn));
  }

  return !required;
}

bool Expression(PNode* theNode, bool required) {
  // expression ::=	number |
  //				method |
  //				variable |
  //				string |
  //				word |
  //				label |
  //				open _expression close

  bool isExpr;
  Symbol* theSym;
  PNode* pn;

  theSym = LookupTok();

  if (symType() == (sym_t)'@') {
    auto* addrof = theNode->newChild(PN_ADDROF);
    isExpr = Expression(addrof, true);
  } else if (IsVar()) {
    UnGetTok();
    isExpr = Variable(theNode);
  } else {
    switch (symType()) {
      case S_NUM:
        theNode->newChild(PN_NUM)->val = symVal();
        isExpr = true;
        break;

      case S_REST:
        theNode->newChild(PN_REST)->val = symVal();
        isExpr = true;
        break;

      case S_SELECT:
        if (theSym)
          Error("Selector %s used as value without #", theSym->name());
        isExpr = false;
        break;

      case S_IDENT:
        // Assume that all unknown identifiers are objects,
        // and fall through to object handling.
        theSym = gSyms.installModule(gSymStr, S_OBJ);
        theSym->clearAn();
        theSym->setObj(nullptr);
        setSymType(S_OBJ);

        //	fall-through

      case S_OBJ:
        // This needs to stay right here, as the handling of
        // S_IDENT falls through to it.
        theNode->newChild(PN_OBJ)->sym = theSym;
        isExpr = true;
        break;

      case S_CLASS:
        pn = theNode->newChild(PN_CLASS);
        if ((uint32_t)symType() == OBJ_SUPER) {
          pn->sym = gClasses[gCurObj->super]->sym;
          pn->val = gClasses[gCurObj->super]->num;
        } else {
          pn->sym = theSym;
          pn->val = theSym->obj()->num;
        }
        isExpr = true;
        break;

      case S_STRING:
        theNode->newChild(PN_STRING)->val = gText.find(gSymStr);
        isExpr = true;
        break;

      case OPEN_P:
        isExpr = _Expression(theNode);
        isExpr = CloseBlock() && isExpr;
        break;

      default:
        if (required)
          Severe("Expression required: %s", gSymStr);
        else
          UnGetTok();
        isExpr = false;
    }
  }

  return isExpr;
}

static bool _Expression(PNode* theNode) {
  // _expression ::=	call |
  //				send |
  //				nary-expr |
  //				binary-expr |
  //				unary-expr |
  //				comp-expr |
  //				script-call |
  //				return |
  //				break |
  //				breakif |
  //				continue |
  //				contif |
  //				while |
  //				repeat |
  //				for |
  //				if |
  //				cond |
  //				switch |
  //				assignment |
  //				inc-dec |
  //				definition |
  //				jump

  bool retVal;
  Symbol* theSym;
  bool oldSelectVar;

  oldSelectVar = gSelectorIsVar;
  gSelectorIsVar = true;

  theSym = LookupTok();

  //    if ( !theSym ) {
  //        printf ( "LookupTok() returned NULL symbol ('%s', %d, %d).\n",
  //        symStr, symType(), curLine );
  //    }

  if (IsProc()) {
    retVal = Call(theNode, theSym);
  }

  else if (IsObj()) {
    retVal = Send(theNode, theSym);
  } else {
    switch (symType()) {
      case S_NARY:
        retVal = NaryExpr(theNode);
        break;

      case S_BINARY:
        retVal = BinaryExpr(theNode);
        break;

      case S_ASSIGN:
        retVal = Assignment(theNode);
        break;

      case S_UNARY:
        retVal = UnaryExpr(theNode);
        break;

      case S_COMP:
        retVal = CompExpr(theNode);
        break;

      case S_REST:
        retVal = Rest(theNode);
        break;

      case S_KEYWORD:
        switch (symVal()) {
          case K_RETURN:
            retVal = Return(theNode);
            break;

          case K_BREAK:
            retVal = Break(theNode);
            break;

          case K_BREAKIF:
            retVal = BreakIf(theNode);
            break;

          case K_CONT:
            retVal = Continue(theNode);
            break;

          case K_CONTIF:
            retVal = ContIf(theNode);
            break;

          case K_WHILE:
            retVal = While(theNode);
            break;

          case K_REPEAT:
            retVal = Repeat(theNode);
            break;

          case K_FOR:
            retVal = For(theNode);
            break;

          case K_IF:
            retVal = If(theNode);
            break;

          case K_COND:
            retVal = Cond(theNode);
            break;

          case K_SWITCH:
            retVal = Switch(theNode);
            break;

          case K_SWITCHTO:
            retVal = SwitchTo(theNode);
            break;

          case K_INC:
          case K_DEC:
            retVal = IncDec(theNode);
            break;

          case K_DEFINE:
            Define();
            retVal = true;
            break;

          case K_ENUM:
            Enum();
            retVal = true;
            break;

          case K_CLASS:
          case K_INSTANCE:
          case K_METHOD:
          case K_PROC:
            // Oops!  Got out of synch!
            Error("Mismatched parentheses!");
            longjmp(gRecoverBuf, 1);

          default:
            Severe("Expected an expression here: %s", gSymStr);
            retVal = true;
        }
        break;

      default:
        Severe("Expected an expression here: %s", gSymStr);
        retVal = true;
    }
  }

  gSelectorIsVar = oldSelectVar;
  return retVal;
}

static bool Return(PNode* theNode) {
  PNode* pn;

  // Add a return node, then look for an optional return expression.
  pn = theNode->newChild(PN_RETURN);
  Expression(pn, OPTIONAL);

  return true;
}

static bool Assignment(PNode* theNode) {
  // assignment ::= assign-op variable expression

  bool retVal = false;

  auto pn = std::make_unique<PNode>(PN_ASSIGN);
  pn->val = symVal();

  // Get the variable
  if (Variable(pn.get()))
    // Get the expression to be assigned to it.
    retVal = Expression(pn.get(), REQUIRED);

  if (retVal) theNode->addChild(std::move(pn));

  return retVal;
}

static bool Call(PNode* theNode, Symbol* theSym) {
  // call ::= procedure-symbol expression*

  bool is_extern = theSym->type == S_EXTERN;
  auto pn = std::make_unique<PNode>((pn_t)(is_extern ? PN_EXTERN : PN_CALL));
  pn->sym = theSym;
  if (!is_extern) {
    pn->val = theSym->val();
  }

  // Collect the arguments
  while (Expression(pn.get(), OPTIONAL))
    ;

  theNode->addChild(std::move(pn));
  return true;
}

static bool Send(PNode* theNode, Symbol* theSym) {
  // send ::= (object | variable) message+

  PNode* pn;
  PNode* dn;
  std::string_view objName;

  pn = theNode->newChild(PN_SEND);

  // Add a node giving the type and value which determines
  // the destination of the send.
  if (symType() == S_CLASS && symHasVal(OBJ_SUPER)) {
    dn = pn->newChild(PN_SUPER);
    dn->sym = gClasses[gCurObj->super]->sym;
    dn->val = gClasses[gCurObj->super]->num;
    objName = "super";

  } else {
    if (theSym && theSym->type == S_IDENT) {
      // If the symbol has not been previously defined, define it as
      // an undefined object in the global symbol table.
      theSym = gSyms.installModule(gSymStr, S_OBJ);
      theSym->clearAn();
      theSym->setObj(nullptr);
    }
    UnGetTok();
    Expression(pn, REQUIRED);
    objName =
        pn->first_child()->sym ? pn->first_child()->sym->name() : "object";
  }

  // Collect the messages to send to the object.
  int nMsgs = 0;
  while (Message(pn, theSym)) nMsgs++;

  if (!nMsgs) {
    Error("No messages sent to %s", objName);
    return false;
  }

  return true;
}

static bool Message(PNode* theNode, Symbol* theSym) {
  Symbol* msgSel;
  bool oldSelectVar;
  bool retVal;
  PNode* pn;
  PNode* node;

  // A variable can be a selector
  oldSelectVar = gSelectorIsVar;
  gSelectorIsVar = true;

  // Get the message selector.  If there is none (we hit a closing paren),
  // we're at the end of a series of sends -- return false.
  msgSel = GetSelector(theSym);
  if (!msgSel)
    retVal = false;
  else {
    // Add the message node to the send.
    pn = theNode->newChild(PN_MSG);

    // Add the selector node.
    if (msgSel->type != S_SELECT) {
      UnGetTok();
      Expression(pn, REQUIRED);
    } else {
      node = pn->newChild(PN_SELECT);
      node->val = msgSel->val();
      node->sym = msgSel;
    }

    //	save off the receiver of these messages
    Object* curReceiver = gReceiver;

    // Collect the arguments of the message.
    int nArgs;
    for (nArgs = 0; Expression(pn, OPTIONAL); nArgs++)
      ;

    // Make sure we're not sending multiple arguments to a property
    if (nArgs > 1 && curReceiver) {
      Selector* sn = curReceiver->findSelectorByNum(msgSel->val());
      assert(sn);
      if (sn->tag != T_LOCAL && sn->tag != T_METHOD)
        Error(
            "More than one argument passed to property:  possible missing "
            "comma");
    }

    retVal = true;
  }

  gSelectorIsVar = oldSelectVar;

  return retVal;
}

static bool While(PNode* theNode) {
  // while ::= 'while' expression expression*

  auto pn = std::make_unique<PNode>(PN_WHILE);

  // Get the conditional expression which drives the loop
  if (!Expression(pn.get(), REQUIRED)) {
    return false;
  }

  // Increment the loop nesting count, then get the statements to be
  // executed in the loop.  Set the loop nesting count back down when
  // we're done.
  ++gLoopNest;
  ExprList(pn.get(), OPTIONAL);
  --gLoopNest;

  theNode->addChild(std::move(pn));
  return true;
}

static bool Repeat(PNode* theNode) {
  // repeat ::= 'repeat' expression+

  PNode* pn;

  pn = theNode->newChild(PN_REPEAT);

  // Increment the loop nesting count, then get the statements to be
  // executed in the loop.  Set the loop nesting count back down when
  // we're done.
  ++gLoopNest;
  ExprList(pn, OPTIONAL);
  --gLoopNest;

  return true;
}

static bool For(PNode* theNode) {
  // for ::=	'for' open expression* close	;initialization
  //			expression ;conditional 			open
  // expression* close ;re-initialization 			expression*
  //;loop body

  auto pn = std::make_unique<PNode>(PN_FOR);

  // Get the initialization for the loop
  if (!OpenBlock()) {
    Severe("Need loop initialization.");
    return false;
  }
  ExprList(pn.get(), OPTIONAL);
  CloseBlock();

  // Get the conditional expression which determines exit from the loop.
  if (!Expression(pn.get(), OPTIONAL)) {
    Severe("Need loop termination.");
    return false;
  }

  // Get the re-initialization of the loop.
  if (!OpenBlock()) {
    Severe("Need loop re-initialization.");
    return false;
  }
  ExprList(pn.get(), OPTIONAL);
  CloseBlock();

  // Increment the loop nesting count, then get the statements to be
  // executed in the loop.  Set the loop nesting count back down when
  // we're done.
  ++gLoopNest;
  ExprList(pn.get(), OPTIONAL);
  --gLoopNest;

  theNode->addChild(std::move(pn));
  return true;
}

static bool Break(PNode* theNode) {
  // break ::= 'break' [number]

  PNode* pn;

  pn = theNode->newChild(PN_BREAK);

  GetToken();
  if (symType() == S_NUM)
    pn->val = symVal();
  else {
    UnGetTok();
    pn->val = 1;
  }

  if (pn->val > gLoopNest)
    Warning("Break level greater than loop nesting count.");

  return true;
}

static bool BreakIf(PNode* theNode) {
  // break ::= 'breakif' expression [number]

  auto pn = std::make_unique<PNode>(PN_BREAKIF);

  // Get the conditional expression.
  if (!Expression(pn.get(), REQUIRED)) {
    Severe("Conditional required in 'breakif'.");
    return false;
  }

  // Get the optional break level.
  GetToken();
  if (symType() == S_NUM)
    pn->val = symVal();
  else {
    UnGetTok();
    pn->val = 1;
  }

  if (pn->val > gLoopNest)
    Warning("Break level greater than loop nesting count.");

  theNode->addChild(std::move(pn));
  return true;
}

static bool Continue(PNode* theNode) {
  // continue ::= 'continue' [number]

  PNode* pn;

  pn = theNode->newChild(PN_CONT);

  GetToken();
  if (symType() == S_NUM)
    pn->val = symVal();
  else {
    UnGetTok();
    pn->val = 1;
  }

  if (pn->val > gLoopNest)
    Warning("Continue level greater than loop nesting count.");

  return true;
}

static bool ContIf(PNode* theNode) {
  // contif ::= 'contif'expression [number]

  auto pn = std::make_unique<PNode>(PN_CONTIF);

  // Get the conditional expression.
  if (!Expression(pn.get(), REQUIRED)) {
    Severe("Conditional required in 'breakif'.");
    return false;
  }

  // Get the optional break level.
  GetToken();
  if (symType() == S_NUM)
    pn->val = symVal();
  else {
    UnGetTok();
    pn->val = 1;
  }

  if (pn->val > gLoopNest)
    Warning("Continue level greater than loop nesting count.");

  theNode->addChild(std::move(pn));
  return true;
}

static bool If(PNode* theNode) {
  // if ::=	'if' expression expression+
  //		['else' expression+]

  auto pn = std::make_unique<PNode>(PN_IF);

  // Get the condition
  if (!Expression(pn.get(), REQUIRED)) {
    return false;
  }

  // Get the true branch
  if (!ExprList(pn.get(), OPTIONAL)) {
    return false;
  }

  // Get the else branch, if it exists
  GetToken();
  if (Keyword() == K_ELSE) {
    if (!ExprList(pn.get(), OPTIONAL)) {
      return false;
    }
  } else
    UnGetTok();

  theNode->addChild(std::move(pn));
  return true;
}

static bool Cond(PNode* theNode) {
  // cond := 'cond' ( open expression expression+ close )+
  //		 [open 'else' expression+ close]

  auto pn = std::make_unique<PNode>(PN_COND);

  GetToken();
  while (OpenP(symType())) {
    // Get the expression which serves as the condition
    GetToken();
    if (Keyword() == K_ELSE)
      pn->newChild(PN_ELSE);
    else {
      UnGetTok();
      if (!Expression(pn.get(), REQUIRED)) {
        return false;
      }
    }

    // Get the [optional] expressions to execute if the condition is true.
    ExprList(pn.get(), OPTIONAL);

    CloseBlock();
    GetToken();
  }

  UnGetTok();

  theNode->addChild(std::move(pn));
  return true;
}

static bool Switch(PNode* theNode) {
  // switch :=	'switch' expression open expression expression+ close
  //			[open 'else' expression+ close]

  auto pn = std::make_unique<PNode>(PN_SWITCH);

  // Get the expression to be switched on
  if (!Expression(pn.get(), REQUIRED)) {
    return false;
  }

  GetToken();
  while (OpenP(symType())) {
    // Get the expression to compare to the switch expression
    GetToken();
    if (Keyword() == K_ELSE)
      pn->newChild(PN_ELSE);
    else {
      UnGetTok();
      if (!Expression(pn.get(), REQUIRED)) {
        return false;
      }
    }

    // Get the expressions to execute if this case is selected.
    ExprList(pn.get(), OPTIONAL);
    CloseBlock();
    GetToken();
  }
  UnGetTok();

  theNode->addChild(std::move(pn));
  return true;
}

static bool SwitchTo(PNode* theNode) {
  // switch :=	'switch' expression open expression expression+ close
  //			[open 'else' expression+ close]

  int switchToVal = 0;

  auto pn = std::make_unique<PNode>(PN_SWITCHTO);

  // Get the expression to be switched on
  if (!Expression(pn.get(), REQUIRED)) {
    return false;
  }

  while (OpenBlock()) {
    pn->newChild(PN_NUM)->val = switchToVal++;
    ExprList(pn.get(), OPTIONAL);
    CloseBlock();
  }

  // replace token that was not a start of block
  UnGetTok();

  theNode->addChild(std::move(pn));
  return true;
}

static bool IncDec(PNode* theNode) {
  //	inc-dec ::=	('++' | '--') variable

  // Get the type of operation.
  auto pn = std::make_unique<PNode>(PN_INCDEC);
  pn->val = symVal();

  // Get the argument
  if (Variable(pn.get())) {
    theNode->addChild(std::move(pn));
    return true;
  } else {
    return false;
  }
}

static bool Variable(PNode* theNode) {
  // variable ::= var-symbol | ('[' var-symbol expression ']')

  PNode* pn;
  Symbol* theSym;

  theSym = LookupTok();
  if (symType() == S_OPEN_BRACKET) return Array(theNode);

  if (!IsVar()) {
    Severe("Variable name expected: %s.", gSymStr);
    return false;
  }
  pn = theNode->newChild(PNType(symType()));
  pn->val = symVal();
  pn->sym = theSym;

  return true;
}

static bool Array(PNode* theNode) {
  PNode* node;

  Symbol* lookupSym = GetSymbol();
  if (lookupSym->type != S_GLOBAL && lookupSym->type != S_LOCAL &&
      lookupSym->type != S_PARM && lookupSym->type != S_TMP) {
    Severe("Array name expected: %s.", gSymStr);
    return false;
  }

  auto pn = std::make_unique<PNode>(PN_INDEX);
  node = pn->newChild(PNType(lookupSym->type));
  node->val = lookupSym->val();
  node->sym = lookupSym;

  // Get the index into the array.
  if (!Expression(pn.get(), REQUIRED)) {
    return false;
  }

  GetToken();
  if (symType() != (sym_t)']') {
    Error("Expected closing ']': %s.", gSymStr);
    return false;
  }

  theNode->addChild(std::move(pn));
  return true;
}

static bool Rest(PNode* theNode) {
  LookupTok();
  if (!IsVar() || symType() != S_PARM) {
    Severe("Variable name expected: %s.", gSymStr);
    return false;
  }
  theNode->newChild(PN_REST)->val = symVal();
  return true;
}

static bool NaryExpr(PNode* theNode) {
  //	nary-expr ::=	nary-op expression expression+
  //	nary-op ::=	'+' | '*' | '^' | '|' | '&' | 'and' | 'or'

  std::unique_ptr<PNode> pn;
  int val;
  bool logicExpr = symVal() == N_AND || symVal() == N_OR;

  if (logicExpr)
    pn = std::make_unique<PNode>(PN_COMP);
  else
    pn = std::make_unique<PNode>(PN_NARY);
  pn->val = symVal();

  // Get the first and second arguments
  if (!Expression(pn.get(), REQUIRED)) {
    return false;
  }
  if (!logicExpr && !Expression(pn.get(), REQUIRED)) {
    return false;
  }

  // Get any optional arguments
  while (Expression(pn.get(), OPTIONAL))
    ;

  // See if there are any constant nodes in the expression.  We end
  // up either pointing at the first constant node or at the end
  // of the list of expressions.
  auto first_const_it = std::ranges::find_if(
      pn->children,
      [](std::unique_ptr<PNode> const& arg) { return arg->type == PN_NUM; });

  // If there is a constant node, collect all constant nodes into
  // that node.
  if (first_const_it != pn->children.end()) {
    auto* arg = first_const_it->get();
    auto write_it = first_const_it;
    write_it++;
    auto read_it = write_it;
    while (read_it != pn->children.end()) {
      if ((*read_it)->type != PN_NUM) {
        if (write_it != read_it) {
          *write_it = std::move(*read_it);
        }
        ++write_it;
        ++read_it;
      } else {
        val = (*read_it)->val;
        switch (pn->val) {
          case N_PLUS:
            arg->val += val;
            break;
          case N_MUL:
            arg->val *= val;
            break;
          case N_BITXOR:
            arg->val ^= val;
            break;
          case N_BITAND:
            arg->val &= val;
            break;
          case N_BITOR:
            arg->val |= val;
            break;
          case N_AND:
            arg->val = arg->val && val;
            break;
          case N_OR:
            arg->val = arg->val || val;
            break;
        }
        ++read_it;
      }
    }

    pn->children.erase(write_it, pn->children.end());
  }

  // If there is only a single constant node remaining, set the
  // passed node to the constant.  Otherwise, leave it as an nary node.
  if (pn->children.size() == 1 && pn->first_child()->type == PN_NUM) {
    pn->type = PN_NUM;
    pn->val = pn->first_child()->val;
    pn->children.clear();
  }

  theNode->addChild(std::move(pn));
  return true;
}

static bool BinaryExpr(PNode* theNode) {
  //	binary-expr ::=	binary-op expression expression
  //	binary-op ::=		'-' | '/' | '<<' | '>>' | '^' | '&' | '|' | '%'

  auto pn = std::make_unique<PNode>(PN_BINARY);
  int opType = pn->val = symVal();

  // Get the first argument.
  if (!Expression(pn.get(), REQUIRED)) {
    return false;
  }

  // Get the second argument.  If the operator is '-' and there is
  // no second argument, it is a unary minus, i.e. a negation.
  if (opType != B_MINUS) {
    if (!Expression(pn.get(), REQUIRED)) {
      return false;
    }

  } else if (!Expression(pn.get(), OPTIONAL)) {
    pn->type = PN_UNARY;
    pn->val = U_NEG;
    auto* arg1 = pn->first_child();
    if (arg1->type == PN_NUM) {
      pn->type = PN_NUM;
      pn->val = -arg1->val;
      pn->children.clear();
    }
  }

  // If both arguments are constants, evaluate the expression and
  // return a constant node with that value.
  auto* arg1 = pn->children[0].get();
  auto* arg2 = pn->children[1].get();
  if (arg1->type == PN_NUM && arg2->type == PN_NUM) {
    pn->type = PN_NUM;
    auto val1 = arg1->val;
    auto val2 = arg2->val;
    switch (pn->val) {
      case B_MINUS:
        pn->val = val1 - val2;
        break;
      case B_DIV:
        if (!val2) {
          Severe("division by zero.");
          return false;
        }
        pn->val = val1 / val2;
        break;
      case B_MOD:
        pn->val = val1 % val2;
        break;
      case B_SLEFT:
        pn->val = val1 << val2;
        break;
      case B_SRIGHT:
        pn->val = val1 >> val2;
        break;
    }
    pn->children.clear();
  }

  theNode->addChild(std::move(pn));
  return true;
}

static bool UnaryExpr(PNode* theNode) {
  //	unary-expr ::=		unary-op expression
  //	unary-op ::=		'~' | '!'

  auto pn = std::make_unique<PNode>(PN_UNARY);
  pn->val = symVal();

  // Get the argument
  if (!Expression(pn.get(), REQUIRED)) {
    return false;
  }

  // If the argument is constant, just do the unary operation on it,
  // otherwise generate a unary operation node.
  auto* theArg = pn->children[0].get();
  if (theArg->type == PN_NUM) {
    pn->type = PN_NUM;
    switch (pn->val) {
      case U_NOT:
        pn->val = !theArg->val ? 1 : 0;
        break;
      case U_BNOT:
        pn->val = theArg->val ^ -1;
        break;
    }
    pn->children.clear();
  }

  theNode->addChild(std::move(pn));
  return true;
}

static bool CompExpr(PNode* theNode) {
  //	comp-expr ::=	comp-op expression expression+
  //	comp-op ::=	'>' | '>=' | '<' | '<=' | '==' | '!='

  auto pn = std::make_unique<PNode>(PN_COMP);
  pn->val = symVal();

  // Get the first and second arguments.
  if (!Expression(pn.get(), REQUIRED)) {
    return false;
  }
  if (!Expression(pn.get(), REQUIRED)) {
    return false;
  }

  // Get any optional arguments
  while (Expression(pn.get(), OPTIONAL))
    ;

  theNode->addChild(std::move(pn));
  return true;
}

pn_t PNType(sym_t st) {
  // Return the Parsenode type for a given symbol type.

  switch (st) {
    case S_CLASS:
      return PN_CLASS;
    case S_OBJ:
      return PN_OBJ;
    case S_SELECT:
    case S_LOCAL:
      return PN_LOCAL;
    case S_GLOBAL:
      return PN_GLOBAL;
    case S_TMP:
      return PN_TMP;
    case S_PARM:
      return PN_PARM;
    case S_PROP:
      return PN_PROP;
    default:
      Fatal("Bad symbol type in PNType().");
      break;
  }

  // Should never be reached.
  Fatal("Bad symbol type in PNType().");
  return (pn_t)0;
}
