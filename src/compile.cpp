//	compile.cpp
// 	compile the object code list for a parse tree

#include "compile.hpp"

#include <ranges>

#include "anode.hpp"
#include "asm.hpp"
#include "define.hpp"
#include "error.hpp"
#include "input.hpp"
#include "object.hpp"
#include "opcodes.hpp"
#include "parse.hpp"
#include "sc.hpp"
#include "text.hpp"

static void MakeAccess(PNode*, uint8_t);
static void MakeImmediate(int);
static void MakeString(PNode*);
static void MakeCall(PNode* pn);
static void MakeClassID(PNode*);
static void MakeObjID(PNode*);
static void MakeSend(PNode*);
static int MakeMessage(PNode::ChildSpan children);
static void MakeProc(PNode*);
static int MakeArgs(PNode::ChildSpan children);
static void MakeUnary(PNode*);
static void MakeBinary(PNode*);
static void MakeNary(PNode*);
static void MakeAssign(PNode*);
static void MakeReturn(PNode*);
static void MakeComp(PNode*);
static void MakeAnd(PNode::ChildSpan);
static void MakeOr(PNode::ChildSpan);
static void MakeIncDec(PNode*);
static void MakeCompOp(int);
static void MakeIf(PNode*);
static void MakeCond(PNode*);
static void MakeSwitch(PNode*);

void Compile(PNode* pn) {
  // Recursively compile code for a given node.

  if (includeDebugInfo && pn->type != PN_PROC && pn->type != PN_METHOD &&
      pn->lineNum > lastLineNum) {
    curList->newNode<ANLineNum>(pn->lineNum);
    lastLineNum = pn->lineNum;
  }

  switch (pn->type) {
    case PN_ELIST:
      // An expression list.  Compile code for each expression
      // in the list.
      for (auto const& child : pn->children) Compile(child.get());
      break;

    case PN_EXPR:
      // Compile the expression.
      Compile(pn->first_child());
      break;

    case PN_ASSIGN:
      MakeAssign(pn);
      break;

    case PN_SELECT:
      MakeImmediate(pn->val);
      break;

    case PN_NUM:
      MakeImmediate(pn->val);
      break;

    case PN_STRING:
      MakeString(pn);
      break;

    case PN_GLOBAL:
    case PN_LOCAL:
    case PN_TMP:
    case PN_PARM:
    case PN_INDEX:
    case PN_PROP:
      // Compile code to load the accumulator from a variable.
      MakeAccess(pn, OP_LDST | OP_LOAD);
      break;

    case PN_ADDROF:
      MakeAccess(pn->first_child(), op_lea);
      break;

    case PN_CLASS:
      MakeClassID(pn);
      break;

    case PN_OBJ:
      MakeObjID(pn);
      break;

    case PN_EXTERN:
    case PN_CALL:
      MakeCall(pn);
      break;

    case PN_SEND:
      MakeSend(pn);
      break;

    case PN_UNARY:
      MakeUnary(pn);
      break;

    case PN_BINARY:
      MakeBinary(pn);
      break;

    case PN_NARY:
      MakeNary(pn);
      break;

    case PN_COMP:
      MakeComp(pn);
      break;

    case PN_RETURN:
      MakeReturn(pn);
      break;

    case PN_IF:
      MakeIf(pn);
      break;

    case PN_COND:
      MakeCond(pn);
      break;

    case PN_SWITCH:
    case PN_SWITCHTO:
      MakeSwitch(pn);
      break;

    case PN_INCDEC:
      MakeIncDec(pn);
      break;

    case PN_PROC:
    case PN_METHOD:
      MakeProc(pn);
      break;

      // The following routines are in 'loop.cpp'.
    case PN_WHILE:
      MakeWhile(pn);
      break;

    case PN_REPEAT:
      MakeRepeat(pn);
      break;

    case PN_FOR:
      MakeFor(pn);
      break;

    case PN_BREAK:
      MakeBreak(pn);
      break;

    case PN_BREAKIF:
      MakeBreakIf(pn);
      break;

    case PN_CONT:
      MakeContinue(pn);
      break;

    case PN_CONTIF:
      MakeContIf(pn);
      break;

    // Do nothing for unhandled node types.
    default:
      Error("Internal error: unhandled node type in Compile(): %d", pn->type);
      break;
  }
}

static void MakeAccess(PNode* pn, uint8_t theCode) {
  // Compile code to access the variable indicated by pn.  Access
  // is OP_STORE or OP_LOAD

  ANVarAccess* an;
  uint16_t theAddr;
  pn_t varType;

  // Check for indexing and compile the index if necessary.
  bool indexed = pn->type == PN_INDEX;
  if (indexed) {
    PNode* child = pn->children[0].get();
    if (theCode == (OP_LDST | OP_STORE))
      curList->newNode<ANOpCode>(
          op_push);                  // push the value to store on the stack
    Compile(pn->children[1].get());  // compile index value
    if (theCode != op_lea) theCode |= OP_INDEX;  // set the indexing bit
    theAddr = child->val;
    varType = child->type;

  } else {
    theAddr = pn->val;
    varType = pn->type;
  }

  // Set the bits indicating the type of variable to be accessed, then
  // put out the opcode to access it.
  if (theCode == op_lea) {
    uint32_t accType;
    switch (varType) {
      case PN_GLOBAL:
        accType = OP_GLOBAL;
        break;
      case PN_LOCAL:
        accType = OP_LOCAL;
        break;
      case PN_TMP:
        accType = OP_TMP;
        break;
      case PN_PARM:
        accType = OP_PARM;
        break;
      default:
        Fatal("Internal error: bad variable type in MakeAccess()");
        break;
    }

    if (indexed) accType |= OP_INDEX;
    an = curList->newNode<ANEffctAddr>(theCode, theAddr, accType);

  } else {
    if (varType == PN_PROP) {
      switch (theCode & OP_TYPE) {
        case OP_LOAD:
          theCode = op_pToa;
          break;
        case OP_STORE:
          theCode = op_aTop;
          break;
        case OP_INC:
          theCode = op_ipToa;
          break;
        case OP_DEC:
          theCode = op_dpToa;
          break;
      }

    } else {
      switch (varType) {
        case PN_GLOBAL:
          theCode |= OP_GLOBAL;
          break;
        case PN_LOCAL:
          theCode |= OP_LOCAL;
          break;
        case PN_TMP:
          theCode |= OP_TMP;
          break;
        case PN_PARM:
          theCode |= OP_PARM;
          break;
        default:
          Fatal("Internal error: bad variable type in MakeAccess()");
          break;
      }
    }

    if (theAddr < 256) theCode |= OP_BYTE;
    an = curList->newNode<ANVarAccess>(theCode, theAddr);
  }

  // Put a pointer to the referenced symbol in the assembly node,
  // so we can print its name in the listing.
  switch (pn->type) {
    case PN_NUM:
      break;
    case PN_INDEX:
      an->sym = pn->child_at(0)->sym;
      break;
    default:
      an->sym = pn->sym;
      break;
  }
}

static void MakeImmediate(int val) {
  curList->newNode<ANOpSign>(op_loadi, val);
}

static void MakeString(PNode* pn) { curList->newNode<ANOpOfs>(pn->val); }

static void MakeCall(PNode* pn) {
  // Compile a call to a procedure, the kernel, etc.

  // Push the number of arguments on the stack (we don't know the
  // number yet).
  ANOpUnsign* an = curList->newNode<ANOpUnsign>(op_pushi, 0);

  // Compile the arguments.
  uint32_t numArgs = MakeArgs(pn->children);

  // Put the number of arguments in its asmNode.
  an->value = numArgs;

  // Compile the call.
  Symbol* sym = pn->sym;
  if (pn->type == PN_CALL) {
    ANCall* call = curList->newNode<ANCall>(sym);

    // If the destination procedure has not yet been defined, add
    // this node to the list of those waiting for its definition.
    if (sym->type == S_PROC && sym->val() == UNDEFINED)
      call->addBackpatch(sym);
    else
      call->setTarget(sym->loc());

    call->numArgs = 2 * numArgs;

  } else {
    Public* pub = sym->ext();
    ANOpExtern* extCall =
        curList->newNode<ANOpExtern>(sym, pub->script, pub->entry);
    extCall->numArgs = 2 * numArgs;
  }
}

static void MakeClassID(PNode* pn) {
  // Compile a class ID.

  ANOpUnsign* an = curList->newNode<ANOpUnsign>(op_class, pn->sym->obj()->num);
  an->sym = pn->sym;
}

static void MakeObjID(PNode* pn) {
  // Compile an object ID.

  if (pn->sym->hasVal(OBJ_SELF))
    curList->newNode<ANOpCode>(op_selfID);

  else {
    Symbol* sym = pn->sym;
    ANObjID* an = curList->newNode<ANObjID>(sym);

    // If the object is not defined yet, add this node to the list
    // of those waiting for the definition.
    if (!sym->obj() || sym->obj() == curObj)
      an->addBackpatch(sym);
    else
      an->setTarget(sym->obj()->an);
  }
}

static void MakeSend(PNode* pn) {
  // Compile code for sending to an object through a handle stored in a
  // variable.

  // Get pointer to object node (an expression)
  PNode* on = pn->children[0].get();

  // Compile the messages to the object.
  int numArgs = 0;
  for (auto const& msg :
       std::ranges::subrange(pn->children.begin() + 1, pn->children.end())) {
    numArgs += MakeMessage(msg->children);
  }

  // Add the appropriate send.
  ANSend* an;
  if (on->type == PN_OBJ && on->val == (int)OBJ_SELF)
    an = curList->newNode<ANSend>(op_self);
  else if (on->type == PN_SUPER)
    an = curList->newNode<ANSuper>(on->sym, on->val);
  else {
    Compile(on);  // compile the object/class id
    an = curList->newNode<ANSend>(op_send);
  }

  an->numArgs = 2 * numArgs;
}

static int MakeMessage(PNode::ChildSpan theMsg) {
  // Compile code to push a message on the stack.

  // Compile the selector.
  Compile(theMsg.front().get());
  curList->newNode<ANOpCode>(op_push);

  // Push the number of arguments on the stack (we don't know the
  // number yet).
  ANOpUnsign* an = curList->newNode<ANOpUnsign>(op_pushi, (uint32_t)-1);

  // Compile the arguments to the message and fix up the number
  // of arguments to the message.
  an->value = MakeArgs(theMsg.subspan(1));

  return an->value + 2;
}

static int MakeArgs(PNode::ChildSpan args) {
  // Compile code to push the arguments to a call on the stack.
  // Return the number of arguments.

  int numArgs = 0;

  for (auto const& arg : args) {
    if (arg->type == PN_REST)
      curList->newNode<ANOpUnsign>(op_rest | OP_BYTE, arg->val);
    else {
      Compile(arg.get());
      curList->newNode<ANOpCode>(op_push);
      ++numArgs;
    }
  }

  return numArgs;
}

static void MakeUnary(PNode* pn) {
  // Compile code for unary operators.

  // Do the argument to the operator.
  Compile(pn->first_child());

  // Put out the appropriate opcode.
  uint16_t theCode;
  switch (pn->val) {
    case U_NEG:
      theCode = op_neg;
      break;
    case U_NOT:
      theCode = op_not;
      break;
    case U_BNOT:
      theCode = op_bnot;
      break;
  }
  curList->newNode<ANOpCode>(theCode);
}

static void MakeBinary(PNode* pn) {
  // Compile code for a binary operator.

  // Compile the arguments, putting the first on the stack.
  Compile(pn->child_at(0));
  curList->newNode<ANOpCode>(op_push);
  Compile(pn->child_at(1));

  // Put out the opcode.
  uint16_t theCode;
  switch (pn->val) {
    case B_MINUS:
      theCode = op_sub;
      break;
    case B_DIV:
      theCode = op_div;
      break;
    case B_SLEFT:
      theCode = op_shl;
      break;
    case B_SRIGHT:
      theCode = op_shr;
      break;
    case B_MOD:
      theCode = op_mod;
      break;
  }
  curList->newNode<ANOpCode>(theCode);
}

static void MakeNary(PNode* pn) {
  // Compile code for an nary operator (one with any number of arguments).

  // Compile the first argument and push its value on the stack.
  Compile(pn->child_at(0));

  for (auto const& arg : pn->rest()) {
    // Push the previous argument on the stack for combining with the
    // next argument.
    curList->newNode<ANOpCode>(op_push);
    // Compile the next argument.
    Compile(arg.get());

    // Put out the appropriate opcode.
    uint16_t theCode;
    switch (pn->val) {
      case N_PLUS:
        theCode = op_add;
        break;
      case N_MUL:
        theCode = op_mul;
        break;
      case N_BITOR:
        theCode = op_or;
        break;
      case N_BITAND:
        theCode = op_and;
        break;
      case N_BITXOR:
        theCode = op_xor;
        break;
    }
    curList->newNode<ANOpCode>(theCode);
  }
}

static void MakeAssign(PNode* pn) {
  // If this is an arithmetic-op assignment, put the value of the
  // target variable on the stack for the operation.
  if (pn->val != A_EQ) {
    MakeAccess(pn->child_at(0), OP_LDST | OP_LOAD);
    curList->newNode<ANOpCode>(op_push);
  }

  // Compile the value to be assigned.
  Compile(pn->child_at(1));

  // If this is an arithmetic-op assignment, do the arithmetic operation.
  uint16_t theCode;
  if (pn->val != A_EQ) {
    switch (pn->val) {
      case A_PLUS:
        theCode = op_add;
        break;
      case A_MUL:
        theCode = op_mul;
        break;
      case A_MINUS:
        theCode = op_sub;
        break;
      case A_DIV:
        theCode = op_div;
        break;
      case A_SLEFT:
        theCode = op_shl;
        break;
      case A_SRIGHT:
        theCode = op_shr;
        break;
      case A_XOR:
        theCode = op_xor;
        break;
      case A_AND:
        theCode = op_and;
        break;
      case A_OR:
        theCode = op_or;
        break;
    }
    curList->newNode<ANOpCode>(theCode);
  }

  MakeAccess(pn->child_at(0), OP_LDST | OP_STORE);
}

static void MakeReturn(PNode* pn) {
  // Compile code for a return.

  // If there was an argument to the return, compile it.
  if (pn->first_child()) Compile(pn->first_child());

  // Put out the return opcode.
  curList->newNode<ANOpCode>(op_ret);
}

void MakeBranch(uint8_t theCode, ANode* bn, Symbol* dest) {
  // Compile code for a branch.  The type of branch is in 'theCode', the
  // destination is 'bn'.  If the the destination is not yet defined,
  // 'dest' will point to a the symbol of the destination.

  ANBranch* an = curList->newNode<ANBranch>(theCode);

  // If the target of the branch has already been defined, point to
  // it.  Otherwise, add this node the the list of those waiting for
  // the target to be defined
  if (bn)
    an->setTarget(bn);
  else if (dest)
    an->addBackpatch(dest);
  else
    Error("MakeBranch: bad call");
}

static void MakeComp(PNode* pn) {
  // Compile a comparison expression.  These expressions are nary expressions
  // with an early out -- the moment the truth value of the expression is
  // known, we stop evaluating the expression.

  // Get the comparison operator.
  uint32_t op = pn->val;

  if (op == N_OR)
    // Handle special case of '||'.
    MakeOr(pn->children);
  else if (op == N_AND)
    // Handle special case of '&&'.
    MakeAnd(pn->children);
  else {
    // Point to the first argument and set up an empty need list (which
    // will be used to keep track of those nodes which need the address
    // of the end of the expression for the early out).
    Symbol earlyOut;

    // Compile the first two operands and do the test.
    Compile(pn->child_at(0));
    curList->newNode<ANOpCode>(op_push);
    Compile(pn->child_at(1));
    MakeCompOp(op);

    // If there are no more operands, we're done.  Otherwise we've got
    // to bail out of the test if it is already false or continue if it
    // is true so far.
    for (auto const& node : pn->rest_at(2)) {
      // Early out if false.
      MakeBranch(op_bnt, 0, &earlyOut);

      // Push the previous accumulator value on the stack in
      // order to continue the comparison.
      curList->newNode<ANOpCode>(op_pprev);

      // Compile the next argument and test it.
      Compile(node.get());
      MakeCompOp(op);
    }

    // Set the target for any branches to the end of the expression
    MakeLabel(&earlyOut);
  }
}

static void MakeAnd(PNode::ChildSpan args) {
  // Compile code for the '&&' operator.

  Symbol earlyOut;

  Compile(args[0].get());

  for (auto const& arg : args.subspan(1)) {
    // Make a branch for an early out if the expression is false.
    MakeBranch(op_bnt, 0, &earlyOut);
    // Compile an argument.
    Compile(arg.get());
  }

  // Set the target for any early-out branches.
  MakeLabel(&earlyOut);
}

static void MakeOr(PNode::ChildSpan args) {
  // Compile code for the '||' operator.

  Symbol earlyOut;

  Compile(args[0].get());

  for (auto const& arg : args.subspan(1)) {
    // Make a branch for an early out if the expression is true.
    MakeBranch(op_bt, 0, &earlyOut);
    // Compile code for an argument.
    Compile(arg.get());
  }

  // Make a target for the early-out branches.
  MakeLabel(&earlyOut);
}

static void MakeCompOp(int op) {
  // Compile the opcode corresponding to the comparison operator 'op'.

  uint16_t theCode;
  switch (op) {
    case C_GT:
      theCode = op_gt;
      break;
    case C_GE:
      theCode = op_ge;
      break;
    case C_LT:
      theCode = op_lt;
      break;
    case C_LE:
      theCode = op_le;
      break;
    case C_EQ:
      theCode = op_eq;
      break;
    case C_NE:
      theCode = op_ne;
      break;
    case C_UGT:
      theCode = op_ugt;
      break;
    case C_UGE:
      theCode = op_uge;
      break;
    case C_ULT:
      theCode = op_ult;
      break;
    case C_ULE:
      theCode = op_ule;
      break;
  }

  curList->newNode<ANOpCode>(theCode);
}

static void MakeIf(PNode* pn) {
  // Compile code for an 'if' statement.

  // Compile the conditional expression
  Compile(pn->child_at(0));

  // Branch to the else code (if there is any) if the expression is false.
  Symbol elseLabel;
  MakeBranch(op_bnt, 0, &elseLabel);

  // Compile the code to be executed if expression was true.
  if (pn->child_at(1)) Compile(pn->child_at(1));

  // If there is no 'else' code, we're done -- backpatch the branch.
  // Otherwise, jump around the else code, backpatch the jump to the
  // else code, compile the else code, and backpatch the jump around
  // the else code.
  if (!pn->child_at(2))
    MakeLabel(&elseLabel);

  else {
    Symbol doneLabel;
    MakeBranch(op_jmp, 0, &doneLabel);
    MakeLabel(&elseLabel);
    Compile(pn->child_at(2));
    MakeLabel(&doneLabel);
  }
}

static void MakeCond(PNode* pn) {
  // Compile code for a 'cond' expression.

  Symbol done;
  Symbol next;
  bool elseSeen = false;

  // Children alternate between conditions and body.
  // Bodies are always an instance of ELIST, which is used to detect if there
  // is any body for each clause.

  std::size_t i = 0;
  while (i < pn->children.size()) {
    auto* condition = pn->children[i++].get();
    PNode* body = i < pn->children.size() && pn->children[i]->type == PN_ELIST
                      ? pn->children[i++].get()
                      : nullptr;
    bool atEnd = i == pn->children.size();
    // The else clause needs no test before its execution.  Otherwise,
    // compile the code to test a condition and branch to the next
    // condition test if the condition is not true.
    if (condition->type != PN_ELSE) {
      if (elseSeen) Error("Else must come at end of cond statement");

      // Compile the condition test.
      Compile(condition);

      if (atEnd && !body) {
        //	if we're at the end, just break out.
        break;
      }

      //	if we're on the last test and it fails, exit
      if (body && atEnd) {
        MakeBranch(op_bnt, 0, &done);
        //	if we're on an interior test and it fails, go to next test
      } else {
        MakeBranch(op_bnt, 0, &next);
      }

    } else if (elseSeen) {
      Error("Multiple else clauses");
    } else {
      elseSeen = true;
    }

    // Compile the statements to be executed if a condition was
    // satisfied.
    if (body) {
      Compile(body);
    }

    // If we're at the end of the cond clause, we're done.  Otherwise
    // make a jump to the end of the cond clause and compile a
    // destination for the jump which branched around the code
    // just compiled.
    if (!atEnd) {
      MakeBranch(op_jmp, 0, &done);
      MakeLabel(&next);
    }
  }

  // Make a destination for jumps to the end of the cond clause.
  MakeLabel(&done);
}

static void MakeSwitch(PNode* pn) {
  // Compile code for the 'switch' statement.

  Symbol done;
  Symbol next;
  bool elseSeen = false;

  PNode::ChildSpan children = pn->children;
  auto* value = children[0].get();
  auto cases = children.subspan(1);

  // Compile the expression to be switched on and put the value on
  // the stack.
  Compile(value);
  curList->newNode<ANOpCode>(op_push);

  std::size_t i = 0;
  while (i < cases.size()) {
    auto* caseClause = cases[i++].get();
    PNode* body = i < cases.size() && cases[i]->type == PN_ELIST
                      ? cases[i++].get()
                      : nullptr;
    bool atEnd = i == cases.size();
    // Compile the expression to compare the switch value to, then
    // test the values for equality.  Make a branch around the
    // code if the expressions are not equal.
    if (caseClause->type != PN_ELSE) {
      if (elseSeen) {
        Error("Else must come at end of switch statement");
      }

      // Duplicate the switch value.
      curList->newNode<ANOpCode>(op_dup);

      // Compile the test value.
      Compile(caseClause);

      // Test for equality.
      curList->newNode<ANOpCode>(op_eq);

      //	if we're at the end, just fall through
      if (atEnd && !body) {
        break;
      }

      //	if we're on the last test and it fails, exit
      if (atEnd && body) {
        MakeBranch(op_bnt, 0, &done);

      } else {
        //	if we're on an interior test and it fails, go to next test
        MakeBranch(op_bnt, 0, &next);
      }

    } else if (elseSeen) {
      Error("Multiple else clauses");
    } else {
      elseSeen = true;
    }

    // Compile the statements to be executed if a switch matched.
    if (body) {
      Compile(body);
    }

    // If we're at the end of the switch expression, we're done.
    // Otherwise, make a jump to the end of the expression, then
    // make a target for the branch around the previous code.
    if (!atEnd) {
      MakeBranch(op_jmp, 0, &done);
      MakeLabel(&next);
    }
  }

  // Compile a target for jumps to the end of the switch expression.
  MakeLabel(&done);

  // Take the switch value off the stack.
  curList->newNode<ANOpCode>(op_toss);
}

static void MakeIncDec(PNode* pn) {
  // Compile code for increment or decrement operators.

  uint16_t theCode;

  switch (pn->val) {
    case K_INC:
      theCode = OP_LDST | OP_INC;
      break;
    case K_DEC:
      theCode = OP_LDST | OP_DEC;
      break;
  }
  MakeAccess(pn->first_child(), (uint8_t)theCode);
}

static void MakeProc(PNode* pn) {
  // Compile code for an entire procedure.

  // Make a procedure node and point to the symbol for the procedure
  // (for listing purposes).
  ANCodeBlk* an = pn->type == PN_PROC
                      ? (ANCodeBlk*)curList->newNode<ANProcCode>(pn->sym)
                      : (ANCodeBlk*)curList->newNode<ANMethCode>(pn->sym);
  an->sym->type = (sym_t)(pn->type == PN_PROC ? S_PROC : S_SELECT);

  // If any nodes already compiled have this procedure as a target,
  // they will be on a list hanging off the procedure's symbol table
  // entry (in the 'ref' property) (compiled by the first reference to the
  // procedure).  Let all these nodes know where this one is.
  if (pn->sym->ref()) {
    pn->sym->ref()->backpatch(an);
  }
  pn->sym->setLoc(an);

  //	procedures and methods get special treatment:  the line number
  //	and file name are set here
  if (includeDebugInfo) {
    curList->newNode<ANLineNum>(pn->lineNum);
    lastLineNum = pn->lineNum;
  }

  // If there are to be any temporary variables, add a link node to
  // create them.
  if (pn->val) curList->newNode<ANOpUnsign>(op_link, pn->val);

  // Compile code for the procedure followed by a return.
  if (pn->child_at(0)) Compile(pn->child_at(0));

  if (includeDebugInfo) {
    assert(curSourceFile);
    curList->newNode<ANLineNum>(curSourceFile->lineNum);
  }
  curList->newNode<ANOpCode>(op_ret);

  an->finish();
}

void MakeDispatch(int maxEntry) {
  // Compile the dispatch table which goes at the start of this script.

  WithCurList withCurList(&dispTbl->entries);

  // Now cycle through the publicly declared procedures/objects,
  // creating asmNodes for a table of their offsets.
  numDispTblEntries->value = maxEntry + 1;
  for (int i = 0; i <= maxEntry; ++i) {
    ANDispatch* an = curList->newNode<ANDispatch>();
    if ((an->sym = FindPublic(i)))
      // Add this to the backpatch list of the symbol.
      an->addBackpatch(an->sym);
  }
}

void MakeObject(Object* theObj) {
  ANOfsProp* pDict = nullptr;
  ANOfsProp* mDict = nullptr;

  {
    WithCurList withCurList(sc->heapList->getList());

    // Create the object ID node.
    ANObject* obj =
        curList->newNodeBefore<ANObject>(nullptr, theObj->sym, theObj->num);
    theObj->an = obj;

    // Create the table of properties.
    ANTable* props = curList->newNodeBefore<ANTable>(nullptr, "properties");

    {
      WithCurList withCurList(&props->entries);

      for (auto* sp : theObj->selectors())
        if (IsProperty(sp)) {
          switch (sp->tag) {
            case T_PROP:
              curList->newNode<ANIntProp>(sp->sym, sp->val);
              break;

            case T_TEXT:
              curList->newNode<ANTextProp>(sp->sym, sp->val);
              break;

            case T_PROPDICT:
              pDict = curList->newNode<ANOfsProp>(sp->sym);
              break;

            case T_METHDICT:
              mDict = curList->newNode<ANOfsProp>(sp->sym);
              break;
          }
        }
    }

    // If any nodes already compiled have this object as a target, they
    // will be on a list hanging off the object's symbol table entry.
    // Let all nodes know where this one is.
    if (obj->sym->ref()) obj->sym->ref()->backpatch(props);
    obj->sym->setLoc(props);
  }

  WithCurList withCurList(sc->hunkList->getList());
  // The rest of the object goes into hunk, as it never changes.
  curList->newNodeBefore<ANObject>(codeStart, theObj->sym, theObj->num);

  // If this a class, add the property dictionary.
  ANObjTable* propDict =
      curList->newNodeBefore<ANObjTable>(codeStart, "property dictionary");
  {
    WithCurList withCurList(&propDict->entries);
    if (theObj->num != OBJECTNUM) {
      for (auto* sp : theObj->selectors())
        if (IsProperty(sp)) curList->newNode<ANWord>(sp->sym->val());
    }
  }
  if (pDict) pDict->target = propDict;

  ANObjTable* methDict =
      curList->newNodeBefore<ANObjTable>(codeStart, "method dictionary");
  {
    WithCurList withCurList(&methDict->entries);
    ANWord* numMeth = curList->newNode<ANWord>((short)0);
    for (auto* sp : theObj->selectors())
      if (sp->tag == T_LOCAL) {
        curList->newNode<ANWord>(sp->sym->val());
        curList->newNode<ANMethod>(sp->sym, (ANMethCode*)sp->an);
        sp->sym->setLoc(nullptr);
        ++numMeth->value;
      }
  }
  if (mDict) mDict->target = methDict;
}

void MakeText() {
  // Add text strings to the assembled code.

  // terminate the object portion of the heap with a null word
  sc->heapList->newNode<ANWord>();
  for (Text* tp : text.items()) sc->heapList->newNode<ANText>(tp);
}

void MakeLabel(Symbol* dest) {
  if (dest->ref()) {
    dest->ref()->backpatch(curList->newNode<ANLabel>());
    dest->setRef(nullptr);
  }
}
