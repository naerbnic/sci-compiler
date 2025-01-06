//	expr.cpp		sc
// 	expressions

#include "define.hpp"
#include "error.hpp"
#include "object.hpp"
#include "parse.hpp"
#include "sc.hpp"
#include "sol.hpp"
#include "symtbl.hpp"
#include "text.hpp"
#include "token.hpp"

extern int curLine;

static int loopNest;

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

  PNode* pn;
  int numExpr;

  pn = new PNode(PN_ELIST);

  // Get the expressions making up the list.  Even if an expression
  // list is required, only the first expression is required.
  for (numExpr = 0; Expression(pn, required); ++numExpr) required = False;

  // If we successfully got an expression, add it to the parse tree,
  // otherwise delete it.
  if (!numExpr)
    delete pn;
  else
    theNode->addChild(pn);

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

  if (IsVar()) {
    UnGetTok();
    isExpr = Variable(theNode);

  } else {
    switch (symType) {
      case S_NUM:
        theNode->addChild(new PNode(PN_NUM))->val = symVal;
        isExpr = True;
        break;

      case S_REST:
        theNode->addChild(new PNode(PN_REST))->val = symVal;
        isExpr = True;
        break;

      case S_SELECT:
        if (theSym) Error("Selector %s used as value without #", theSym->name);
        isExpr = False;
        break;

      case S_IDENT:
        // Assume that all unknown identifiers are objects,
        // and fall through to object handling.
        theSym = syms.installModule(symStr, S_OBJ);
        theSym->an = NULL;
        theSym->obj = NULL;
        symType = S_OBJ;

        //	fall-through

      case S_OBJ:
        // This needs to stay right here, as the handling of
        // S_IDENT falls through to it.
        theNode->addChild(new PNode(PN_OBJ))->sym = theSym;
        isExpr = True;
        break;

      case S_CLASS:
        pn = theNode->addChild(new PNode(PN_CLASS));
        if ((uint32_t)symType == OBJ_SUPER) {
          pn->sym = classes[curObj->super]->sym;
          pn->val = classes[curObj->super]->num;
        } else {
          pn->sym = theSym;
          pn->val = symObj->num;
        }
        isExpr = True;
        break;

      case S_STRING:
        theNode->addChild(new PNode(PN_STRING))->val = text.find(symStr);
        isExpr = True;
        break;

      case OPEN_P:
        isExpr = _Expression(theNode);
        isExpr = CloseBlock() && isExpr;
        break;

      default:
        if (required)
          Severe("Expression required: %s", symStr);
        else
          UnGetTok();
        isExpr = False;
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

  oldSelectVar = selectorIsVar;
  selectorIsVar = True;

  theSym = LookupTok();

  //    if ( !theSym ) {
  //        printf ( "LookupTok() returned NULL symbol ('%s', %d, %d).\n",
  //        symStr, symType, curLine );
  //    }

  if (IsProc()) {
    retVal = Call(theNode, theSym);
  }

  else if (IsObj()) {
    retVal = Send(theNode, theSym);
  } else {
    switch (symType) {
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
        switch (symVal) {
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
            retVal = True;
            break;

          case K_ENUM:
            Enum();
            retVal = True;
            break;

          case K_CLASS:
          case K_INSTANCE:
          case K_METHOD:
          case K_PROC:
            // Oops!  Got out of synch!
            Error("Mismatched parentheses!");
            longjmp(recoverBuf, 1);

          default:
            Severe("Expected an expression here: %s", symStr);
            retVal = True;
        }
        break;

      default:
        Severe("Expected an expression here: %s", symStr);
        retVal = True;
    }
  }

  selectorIsVar = oldSelectVar;
  return retVal;
}

static bool Return(PNode* theNode) {
  PNode* pn;

  // Add a return node, then look for an optional return expression.
  pn = theNode->addChild(new PNode(PN_RETURN));
  Expression(pn, OPTIONAL);

  return True;
}

static bool Assignment(PNode* theNode) {
  // assignment ::= assign-op variable expression

  bool retVal = False;
  PNode* pn;

  pn = new PNode(PN_ASSIGN);
  pn->val = symVal;

  // Get the variable
  if (Variable(pn))
    // Get the expression to be assigned to it.
    retVal = Expression(pn, REQUIRED);

  if (retVal)
    theNode->addChild(pn);
  else
    delete pn;

  return retVal;
}

static bool Call(PNode* theNode, Symbol* theSym) {
  // call ::= procedure-symbol expression*

  PNode* pn;

  pn = new PNode((pn_t)(theSym->type == S_EXTERN ? PN_EXTERN : PN_CALL));
  pn->val = theSym->val;
  pn->sym = theSym;

  // Collect the arguments
  while (Expression(pn, OPTIONAL));

  theNode->addChild(pn);
  return True;
}

static bool Send(PNode* theNode, Symbol* theSym) {
  // send ::= (object | variable) message+

  PNode* pn;
  PNode* dn;
  const char* objName;

  pn = theNode->addChild(new PNode(PN_SEND));

  // Add a node giving the type and value which determines
  // the destination of the send.
  if (symType == S_CLASS && symVal == (int)OBJ_SUPER) {
    dn = pn->addChild(new PNode(PN_SUPER));
    dn->sym = classes[curObj->super]->sym;
    dn->val = classes[curObj->super]->num;
    objName = "super";

  } else {
    if (theSym && theSym->type == S_IDENT) {
      // If the symbol has not been previously defined, define it as
      // an undefined object in the global symbol table.
      theSym = syms.installModule(symStr, S_OBJ);
      theSym->an = NULL;
      theSym->obj = NULL;
    }
    UnGetTok();
    Expression(pn, REQUIRED);
    objName = pn->child->sym ? pn->child->sym->name : "object";
  }

  // Collect the messages to send to the object.
  int nMsgs = 0;
  while (Message(pn, theSym)) nMsgs++;

  if (!nMsgs) {
    Error("No messages sent to %s", objName);
    return False;
  }

  return True;
}

static bool Message(PNode* theNode, Symbol* theSym) {
  Symbol* msgSel;
  bool oldSelectVar;
  bool retVal;
  PNode* pn;
  PNode* node;

  // A variable can be a selector
  oldSelectVar = selectorIsVar;
  selectorIsVar = True;

  // Get the message selector.  If there is none (we hit a closing paren),
  // we're at the end of a series of sends -- return False.
  msgSel = GetSelector(theSym);
  if (!msgSel)
    retVal = False;
  else {
    // Add the message node to the send.
    pn = theNode->addChild(new PNode(PN_MSG));

    // Add the selector node.
    if (msgSel->type != S_SELECT) {
      UnGetTok();
      Expression(pn, REQUIRED);
    } else {
      node = pn->addChild(new PNode(PN_SELECT));
      node->val = msgSel->val;
      node->sym = msgSel;
    }

    //	save off the receiver of these messages
    Object* curReceiver = receiver;

    // Collect the arguments of the message.
    int nArgs;
    for (nArgs = 0; Expression(pn, OPTIONAL); nArgs++);

    // Make sure we're not sending multiple arguments to a property
    if (nArgs > 1 && curReceiver) {
      Selector* sn = curReceiver->findSelector(msgSel);
      assert(sn);
      if (sn->tag != T_LOCAL && sn->tag != T_METHOD)
        Error(
            "More than one argument passed to property:  possible missing "
            "comma");
    }

    retVal = True;
  }

  selectorIsVar = oldSelectVar;

  return retVal;
}

static bool While(PNode* theNode) {
  // while ::= 'while' expression expression*

  PNode* pn;

  pn = new PNode(PN_WHILE);

  // Get the conditional expression which drives the loop
  if (!Expression(pn, REQUIRED)) {
    delete pn;
    return False;
  }

  // Increment the loop nesting count, then get the statements to be
  // executed in the loop.  Set the loop nesting count back down when
  // we're done.
  ++loopNest;
  ExprList(pn, OPTIONAL);
  --loopNest;

  theNode->addChild(pn);
  return True;
}

static bool Repeat(PNode* theNode) {
  // repeat ::= 'repeat' expression+

  PNode* pn;

  pn = theNode->addChild(new PNode(PN_REPEAT));

  // Increment the loop nesting count, then get the statements to be
  // executed in the loop.  Set the loop nesting count back down when
  // we're done.
  ++loopNest;
  ExprList(pn, OPTIONAL);
  --loopNest;

  return True;
}

static bool For(PNode* theNode) {
  // for ::=	'for' open expression* close	;initialization
  //			expression ;conditional 			open
  // expression* close ;re-initialization 			expression*
  //;loop body

  PNode* pn;

  pn = new PNode(PN_FOR);

  // Get the initialization for the loop
  if (!OpenBlock()) {
    Severe("Need loop initialization.");
    delete pn;
    return False;
  }
  ExprList(pn, OPTIONAL);
  CloseBlock();

  // Get the conditional expression which determines exit from the loop.
  if (!Expression(pn, OPTIONAL)) {
    Severe("Need loop termination.");
    delete pn;
    return False;
  }

  // Get the re-initialization of the loop.
  if (!OpenBlock()) {
    Severe("Need loop re-initialization.");
    delete pn;
    return False;
  }
  ExprList(pn, OPTIONAL);
  CloseBlock();

  // Increment the loop nesting count, then get the statements to be
  // executed in the loop.  Set the loop nesting count back down when
  // we're done.
  ++loopNest;
  ExprList(pn, OPTIONAL);
  --loopNest;

  theNode->addChild(pn);
  return True;
}

static bool Break(PNode* theNode) {
  // break ::= 'break' [number]

  PNode* pn;

  pn = theNode->addChild(new PNode(PN_BREAK));

  GetToken();
  if (symType == S_NUM)
    pn->val = symVal;
  else {
    UnGetTok();
    pn->val = 1;
  }

  if (pn->val > loopNest)
    Warning("Break level greater than loop nesting count.");

  return True;
}

static bool BreakIf(PNode* theNode) {
  // break ::= 'breakif' expression [number]

  PNode* pn;

  pn = new PNode(PN_BREAKIF);

  // Get the conditional expression.
  if (!Expression(pn, REQUIRED)) {
    Severe("Conditional required in 'breakif'.");
    delete pn;
    return False;
  }

  // Get the optional break level.
  GetToken();
  if (symType == S_NUM)
    pn->val = symVal;
  else {
    UnGetTok();
    pn->val = 1;
  }

  if (pn->val > loopNest)
    Warning("Break level greater than loop nesting count.");

  theNode->addChild(pn);
  return True;
}

static bool Continue(PNode* theNode) {
  // continue ::= 'continue' [number]

  PNode* pn;

  pn = theNode->addChild(new PNode(PN_CONT));

  GetToken();
  if (symType == S_NUM)
    pn->val = symVal;
  else {
    UnGetTok();
    pn->val = 1;
  }

  if (pn->val > loopNest)
    Warning("Continue level greater than loop nesting count.");

  return True;
}

static bool ContIf(PNode* theNode) {
  // contif ::= 'contif'expression [number]

  PNode* pn;

  pn = new PNode(PN_CONTIF);

  // Get the conditional expression.
  if (!Expression(pn, REQUIRED)) {
    Severe("Conditional required in 'breakif'.");
    delete pn;
    return False;
  }

  // Get the optional break level.
  GetToken();
  if (symType == S_NUM)
    pn->val = symVal;
  else {
    UnGetTok();
    pn->val = 1;
  }

  if (pn->val > loopNest)
    Warning("Continue level greater than loop nesting count.");

  theNode->addChild(pn);
  return True;
}

static bool If(PNode* theNode) {
  // if ::=	'if' expression expression+
  //		['else' expression+]

  PNode* pn;

  pn = new PNode(PN_IF);

  // Get the condition
  if (!Expression(pn, REQUIRED)) {
    delete pn;
    return False;
  }

  // Get the true branch
  if (!ExprList(pn, OPTIONAL)) {
    delete pn;
    return False;
  }

  // Get the else branch, if it exists
  GetToken();
  if (Keyword() == K_ELSE) {
    if (!ExprList(pn, OPTIONAL)) {
      delete pn;
      return False;
    }
  } else
    UnGetTok();

  theNode->addChild(pn);
  return True;
}

static bool Cond(PNode* theNode) {
  // cond := 'cond' ( open expression expression+ close )+
  //		 [open 'else' expression+ close]

  PNode* pn;

  pn = new PNode(PN_COND);

  GetToken();
  while (OpenP(symType)) {
    // Get the expression which serves as the condition
    GetToken();
    if (Keyword() == K_ELSE)
      pn->addChild(new PNode(PN_ELSE));
    else {
      UnGetTok();
      if (!Expression(pn, REQUIRED)) {
        delete pn;
        return False;
      }
    }

    // Get the [optional] expressions to execute if the condition is true.
    ExprList(pn, OPTIONAL);

    CloseBlock();
    GetToken();
  }

  UnGetTok();

  theNode->addChild(pn);
  return True;
}

static bool Switch(PNode* theNode) {
  // switch :=	'switch' expression open expression expression+ close
  //			[open 'else' expression+ close]

  PNode* pn;

  pn = new PNode(PN_SWITCH);

  // Get the expression to be switched on
  if (!Expression(pn, REQUIRED)) {
    delete pn;
    return False;
  }

  GetToken();
  while (OpenP(symType)) {
    // Get the expression to compare to the switch expression
    GetToken();
    if (Keyword() == K_ELSE)
      pn->addChild(new PNode(PN_ELSE));
    else {
      UnGetTok();
      if (!Expression(pn, REQUIRED)) {
        delete pn;
        return False;
      }
    }

    // Get the expressions to execute if this case is selected.
    ExprList(pn, OPTIONAL);
    CloseBlock();
    GetToken();
  }
  UnGetTok();

  theNode->addChild(pn);
  return True;
}

static bool SwitchTo(PNode* theNode) {
  // switch :=	'switch' expression open expression expression+ close
  //			[open 'else' expression+ close]

  PNode* pn;
  int switchToVal = 0;

  pn = new PNode(PN_SWITCHTO);

  // Get the expression to be switched on
  if (!Expression(pn, REQUIRED)) {
    delete pn;
    return False;
  }

  while (OpenBlock()) {
    pn->addChild(new PNode(PN_NUM))->val = switchToVal++;
    ExprList(pn, OPTIONAL);
    CloseBlock();
  }

  // replace token that was not a start of block
  UnGetTok();

  theNode->addChild(pn);
  return True;
}

static bool IncDec(PNode* theNode) {
  //	inc-dec ::=	('++' | '--') variable

  PNode* pn;

  // Get the type of operation.
  pn = new PNode(PN_INCDEC);
  pn->val = symVal;

  // Get the argument
  if (Variable(pn)) {
    theNode->addChild(pn);
    return True;
  } else {
    delete pn;
    return False;
  }
}

static bool Variable(PNode* theNode) {
  // variable ::= var-symbol | ('[' var-symbol expression ']')

  PNode* pn;
  Symbol* theSym;

  theSym = LookupTok();
  if (symType == S_OPEN_BRACKET) return Array(theNode);

  if (!IsVar()) {
    Severe("Variable name expected: %s.", symStr);
    return False;
  }
  pn = theNode->addChild(new PNode(PNType(symType)));
  pn->val = symVal;
  pn->sym = theSym;

  return True;
}

static bool Array(PNode* theNode) {
  PNode* pn;
  PNode* node;

  GetSymbol();
  if (symType != S_GLOBAL && symType != S_LOCAL && symType != S_PARM &&
      symType != S_TMP) {
    Severe("Array name expected: %s.", symStr);
    return False;
  }

  pn = new PNode(PN_INDEX);
  node = pn->addChild(new PNode(PNType(symType)));
  node->val = symVal;
  node->sym = &tokSym;

  // Get the index into the array.
  if (!Expression(pn, REQUIRED)) {
    delete pn;
    return False;
  }

  GetToken();
  if (symType != (sym_t)']') {
    Error("Expected closing ']': %s.", symStr);
    delete pn;
    return False;
  }

  theNode->addChild(pn);
  return True;
}

static bool Rest(PNode* theNode) {
  LookupTok();
  if (!IsVar() || symType != S_PARM) {
    Severe("Variable name expected: %s.", symStr);
    return False;
  }
  theNode->addChild(new PNode(PN_REST))->val = symVal;
  return True;
}

static bool NaryExpr(PNode* theNode) {
  //	nary-expr ::=	nary-op expression expression+
  //	nary-op ::=	'+' | '*' | '^' | '|' | '&' | 'and' | 'or'

  PNode* pn;
  PNode* arg;
  PNode* cur;
  PNode* prev;
  int val;
  bool logicExpr = symVal == N_AND || symVal == N_OR;

  if (logicExpr)
    pn = new PNode(PN_COMP);
  else
    pn = new PNode(PN_NARY);
  pn->val = symVal;

  // Get the first and second arguments
  if (!Expression(pn, REQUIRED)) {
    delete pn;
    return False;
  }
  if (!logicExpr && !Expression(pn, REQUIRED)) {
    delete pn;
    return False;
  }

  // Get any optional arguments
  while (Expression(pn, OPTIONAL));

  // See if there are any constant nodes in the expression.  We end
  // up either pointing at the first constant node or at the end
  // of the list of expressions.
  for (arg = pn->child; arg && arg->type != PN_NUM; arg = arg->next);

  // If there is a constant node, collect all constant nodes into
  // that node.
  if (arg) {
    for (prev = arg, cur = prev->next; cur;) {
      if (cur->type != PN_NUM) {
        prev = cur;
        cur = cur->next;
      } else {
        val = cur->val;
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
        prev->next = cur->next;
        delete cur;
        cur = prev->next;
      }
    }
  }

  // If there is only a single constant node remaining, set the
  // passed node to the constant.  Otherwise, leave it as an nary node.
  arg = pn->child;
  if (arg->type == PN_NUM && !arg->next) {
    pn->type = PN_NUM;
    pn->val = arg->val;
    pn->child = NULL;
    delete arg;
  }

  theNode->addChild(pn);
  return True;
}

static bool BinaryExpr(PNode* theNode) {
  //	binary-expr ::=	binary-op expression expression
  //	binary-op ::=		'-' | '/' | '<<' | '>>' | '^' | '&' | '|' | '%'

  PNode* pn;
  PNode* arg1;
  PNode* arg2;
  int val1;
  int val2;
  int opType;

  pn = new PNode(PN_BINARY);
  opType = pn->val = symVal;

  // Get the first argument.
  if (!Expression(pn, REQUIRED)) {
    delete pn;
    return False;
  }

  // Get the second argument.  If the operator is '-' and there is
  // no second argument, it is a unary minus, i.e. a negation.
  if (opType != B_MINUS) {
    if (!Expression(pn, REQUIRED)) {
      delete pn;
      return False;
    }

  } else if (!Expression(pn, OPTIONAL)) {
    pn->type = PN_UNARY;
    pn->val = U_NEG;
    arg1 = pn->child;
    if (arg1->type == PN_NUM) {
      pn->type = PN_NUM;
      pn->val = -arg1->val;
      delete arg1;
    }
  }

  // If both arguments are constants, evaluate the expression and
  // return a constant node with that value.
  arg1 = pn->child;
  arg2 = arg1->next;
  if (arg1->type == PN_NUM && arg2->type == PN_NUM) {
    pn->type = PN_NUM;
    val1 = arg1->val;
    val2 = arg2->val;
    switch (pn->val) {
      case B_MINUS:
        pn->val = val1 - val2;
        break;
      case B_DIV:
        if (!val2) {
          Severe("division by zero.");
          delete pn;
          return False;
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
    delete arg2;
    delete arg1;
    pn->child = 0;
  }

  theNode->addChild(pn);
  return True;
}

static bool UnaryExpr(PNode* theNode) {
  //	unary-expr ::=		unary-op expression
  //	unary-op ::=		'~' | '!'

  PNode* pn;
  PNode* theArg;

  pn = new PNode(PN_UNARY);
  pn->val = symVal;

  // Get the argument
  if (!Expression(pn, REQUIRED)) {
    delete pn;
    return False;
  }

  // If the argument is constant, just do the unary operation on it,
  // otherwise generate a unary operation node.
  theArg = pn->child;
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
    delete theArg;
    pn->child = NULL;
  }

  theNode->addChild(pn);
  return True;
}

static bool CompExpr(PNode* theNode) {
  //	comp-expr ::=	comp-op expression expression+
  //	comp-op ::=	'>' | '>=' | '<' | '<=' | '==' | '!='

  PNode* pn;

  pn = new PNode(PN_COMP);
  pn->val = symVal;

  // Get the first and second arguments.
  if (!Expression(pn, REQUIRED)) {
    delete pn;
    return False;
  }
  if (!Expression(pn, REQUIRED)) {
    delete pn;
    return False;
  }

  // Get any optional arguments
  while (Expression(pn, OPTIONAL));

  theNode->addChild(pn);
  return True;
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
