//	compile.cpp
// 	compile the object code list for a parse tree

#include "scic/compile.hpp"

#include <cstddef>
#include <cstdint>
#include <ranges>
#include <string>

#include "scic/alist.hpp"
#include "scic/anode.hpp"
#include "scic/asm.hpp"
#include "scic/config.hpp"
#include "scic/define.hpp"
#include "scic/error.hpp"
#include "scic/global_compiler.hpp"
#include "scic/input.hpp"
#include "scic/loop.hpp"
#include "scic/object.hpp"
#include "scic/opcodes.hpp"
#include "scic/parse_context.hpp"
#include "scic/pnode.hpp"
#include "scic/public.hpp"
#include "scic/reference.hpp"
#include "scic/symtypes.hpp"
#include "scic/text.hpp"

static void MakeAccess(AOpList* curList, PNode*, uint8_t);
static void MakeImmediate(AOpList* curList, int);
static void MakeString(AOpList* curList, PNode*);
static void MakeCall(AOpList* curList, PNode* pn);
static void MakeClassID(AOpList* curList, PNode*);
static void MakeObjID(AOpList* curList, PNode*);
static void MakeSend(AOpList* curList, PNode*);
static int MakeMessage(AOpList* curList, PNode::ChildSpan children);
static void MakeProc(AList* curList, PNode*);
static int MakeArgs(AOpList* curList, PNode::ChildSpan children);
static void MakeUnary(AOpList* curList, PNode*);
static void MakeBinary(AOpList* curList, PNode*);
static void MakeNary(AOpList* curList, PNode*);
static void MakeAssign(AOpList* curList, PNode*);
static void MakeReturn(AOpList* curList, PNode*);
static void MakeComp(AOpList* curList, PNode*);
static void MakeAnd(AOpList* curList, PNode::ChildSpan);
static void MakeOr(AOpList* curList, PNode::ChildSpan);
static void MakeIncDec(AOpList* curList, PNode*);
static void MakeCompOp(AOpList* curList, int);
static void MakeIf(AOpList* curList, PNode*);
static void MakeCond(AOpList* curList, PNode*);
static void MakeSwitch(AOpList* curList, PNode*);

void CompileProc(AList* curList, PNode* pn) {
  // Recursively compile code for a given node.

  switch (pn->type) {
    case PN_PROC:
    case PN_METHOD:
      MakeProc(curList, pn);
      break;

    // Do nothing for unhandled node types.
    default:
      Error("Internal error: unhandled node type in Compile(): %d", pn->type);
      break;
  }
}

void CompileExpr(AOpList* curList, PNode* pn) {
  // Recursively compile code for a given node.

  if (gConfig->includeDebugInfo && pn->type != PN_PROC &&
      pn->type != PN_METHOD && pn->lineNum > gLastLineNum) {
    curList->newNode<ANLineNum>(pn->lineNum);
    gLastLineNum = pn->lineNum;
  }

  switch (pn->type) {
    case PN_ELIST:
      // An expression list.  Compile code for each expression
      // in the list.
      for (auto const& child : pn->children) CompileExpr(curList, child.get());
      break;

    case PN_EXPR:
      // Compile the expression.
      CompileExpr(curList, pn->first_child());
      break;

    case PN_ASSIGN:
      MakeAssign(curList, pn);
      break;

    case PN_SELECT:
      MakeImmediate(curList, pn->val);
      break;

    case PN_NUM:
      MakeImmediate(curList, pn->val);
      break;

    case PN_STRING:
      MakeString(curList, pn);
      break;

    case PN_GLOBAL:
    case PN_LOCAL:
    case PN_TMP:
    case PN_PARM:
    case PN_INDEX:
    case PN_PROP:
      // Compile code to load the accumulator from a variable.
      MakeAccess(curList, pn, OP_LDST | OP_LOAD);
      break;

    case PN_ADDROF:
      MakeAccess(curList, pn->first_child(), op_lea);
      break;

    case PN_CLASS:
      MakeClassID(curList, pn);
      break;

    case PN_OBJ:
      MakeObjID(curList, pn);
      break;

    case PN_EXTERN:
    case PN_CALL:
      MakeCall(curList, pn);
      break;

    case PN_SEND:
      MakeSend(curList, pn);
      break;

    case PN_UNARY:
      MakeUnary(curList, pn);
      break;

    case PN_BINARY:
      MakeBinary(curList, pn);
      break;

    case PN_NARY:
      MakeNary(curList, pn);
      break;

    case PN_COMP:
      MakeComp(curList, pn);
      break;

    case PN_RETURN:
      MakeReturn(curList, pn);
      break;

    case PN_IF:
      MakeIf(curList, pn);
      break;

    case PN_COND:
      MakeCond(curList, pn);
      break;

    case PN_SWITCH:
    case PN_SWITCHTO:
      MakeSwitch(curList, pn);
      break;

    case PN_INCDEC:
      MakeIncDec(curList, pn);
      break;

      // The following routines are in 'loop.cpp'.
    case PN_WHILE:
      MakeWhile(curList, pn);
      break;

    case PN_REPEAT:
      MakeRepeat(curList, pn);
      break;

    case PN_FOR:
      MakeFor(curList, pn);
      break;

    case PN_BREAK:
      MakeBreak(curList, pn);
      break;

    case PN_BREAKIF:
      MakeBreakIf(curList, pn);
      break;

    case PN_CONT:
      MakeContinue(curList, pn);
      break;

    case PN_CONTIF:
      MakeContIf(curList, pn);
      break;

    // Do nothing for unhandled node types.
    default:
      Error("Internal error: unhandled node type in Compile(): %d", pn->type);
      break;
  }
}

static void MakeAccess(AOpList* curList, PNode* pn, uint8_t theCode) {
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
          op_push);  // push the value to store on the stack
    CompileExpr(curList, pn->children[1].get());  // compile index value
    if (theCode != op_lea) theCode |= OP_INDEX;   // set the indexing bit
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
    case PN_INDEX: {
      auto* index = pn->child_at(0)->sym;
      if (index) {
        an->name = std::string(index->name());
      }
      break;
    }
    default:
      if (pn->sym) {
        an->name = std::string(pn->sym->name());
      }
      break;
  }
}

static void MakeImmediate(AOpList* curList, int val) {
  curList->newNode<ANOpSign>(op_loadi, val);
}

static void MakeString(AOpList* curList, PNode* pn) {
  curList->newNode<ANOpOfs>(pn->val);
}

static void MakeCall(AOpList* curList, PNode* pn) {
  // Compile a call to a procedure, the kernel, etc.

  // Push the number of arguments on the stack (we don't know the
  // number yet).
  ANOpUnsign* an = curList->newNode<ANOpUnsign>(op_pushi, 0);

  // Compile the arguments.
  uint32_t numArgs = MakeArgs(curList, pn->children);

  // Put the number of arguments in its asmNode.
  an->value = numArgs;

  // Compile the call.
  Symbol* sym = pn->sym;
  if (pn->type == PN_CALL) {
    ANCall* call = curList->newNode<ANCall>(std::string(sym->name()));

    // If the destination procedure has not yet been defined, add
    // this node to the list of those waiting for its definition.
    sym->forwardRef.RegisterCallback(
        [call](ANode* target) { call->target = target; });
    call->numArgs = 2 * numArgs;

  } else {
    Public* pub = sym->ext();
    ANOpExtern* extCall = curList->newNode<ANOpExtern>(std::string(sym->name()),
                                                       pub->script, pub->entry);
    extCall->numArgs = 2 * numArgs;
  }
}

static void MakeClassID(AOpList* curList, PNode* pn) {
  // Compile a class ID.

  ANOpUnsign* an = curList->newNode<ANOpUnsign>(op_class, pn->sym->obj()->num);
  an->name = std::string(pn->sym->name());
}

static void MakeObjID(AOpList* curList, PNode* pn) {
  // Compile an object ID.

  if (pn->sym->hasVal(OBJ_SELF))
    curList->newNode<ANOpCode>(op_selfID);

  else {
    Symbol* sym = pn->sym;
    ANObjID* an = curList->newNode<ANObjID>(sym);

    // If the object is not defined yet, add this node to the list
    // of those waiting for the definition.
    if (!sym->obj() || sym->obj() == gCurObj) {
      sym->forwardRef.RegisterCallback(
          [an](ANode* target) { an->target = target; });
    } else {
      an->target = sym->obj()->an;
    }
  }
}

static void MakeSend(AOpList* curList, PNode* pn) {
  // Compile code for sending to an object through a handle stored in a
  // variable.

  // Get pointer to object node (an expression)
  PNode* on = pn->children[0].get();

  // Compile the messages to the object.
  int numArgs = 0;
  for (auto const& msg :
       std::ranges::subrange(pn->children.begin() + 1, pn->children.end())) {
    numArgs += MakeMessage(curList, msg->children);
  }

  // Add the appropriate send.
  ANSend* an;
  if (on->type == PN_OBJ && on->val == (int)OBJ_SELF)
    an = curList->newNode<ANSend>(op_self);
  else if (on->type == PN_SUPER)
    an = curList->newNode<ANSuper>(std::string(on->sym->name()), on->val);
  else {
    CompileExpr(curList, on);  // compile the object/class id
    an = curList->newNode<ANSend>(op_send);
  }

  an->numArgs = 2 * numArgs;
}

static int MakeMessage(AOpList* curList, PNode::ChildSpan theMsg) {
  // Compile code to push a message on the stack.

  // Compile the selector.
  CompileExpr(curList, theMsg.front().get());
  curList->newNode<ANOpCode>(op_push);

  // Push the number of arguments on the stack (we don't know the
  // number yet).
  ANOpUnsign* an = curList->newNode<ANOpUnsign>(op_pushi, (uint32_t)-1);

  // Compile the arguments to the message and fix up the number
  // of arguments to the message.
  an->value = MakeArgs(curList, theMsg.subspan(1));

  return an->value + 2;
}

static int MakeArgs(AOpList* curList, PNode::ChildSpan args) {
  // Compile code to push the arguments to a call on the stack.
  // Return the number of arguments.

  int numArgs = 0;

  for (auto const& arg : args) {
    if (arg->type == PN_REST)
      curList->newNode<ANOpUnsign>(op_rest | OP_BYTE, arg->val);
    else {
      CompileExpr(curList, arg.get());
      curList->newNode<ANOpCode>(op_push);
      ++numArgs;
    }
  }

  return numArgs;
}

static void MakeUnary(AOpList* curList, PNode* pn) {
  // Compile code for unary operators.

  // Do the argument to the operator.
  CompileExpr(curList, pn->first_child());

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

static void MakeBinary(AOpList* curList, PNode* pn) {
  // Compile code for a binary operator.

  // Compile the arguments, putting the first on the stack.
  CompileExpr(curList, pn->child_at(0));
  curList->newNode<ANOpCode>(op_push);
  CompileExpr(curList, pn->child_at(1));

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

static void MakeNary(AOpList* curList, PNode* pn) {
  // Compile code for an nary operator (one with any number of arguments).

  // Compile the first argument and push its value on the stack.
  CompileExpr(curList, pn->child_at(0));

  for (auto const& arg : pn->rest()) {
    // Push the previous argument on the stack for combining with the
    // next argument.
    curList->newNode<ANOpCode>(op_push);
    // Compile the next argument.
    CompileExpr(curList, arg.get());

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

static void MakeAssign(AOpList* curList, PNode* pn) {
  // If this is an arithmetic-op assignment, put the value of the
  // target variable on the stack for the operation.
  if (pn->val != A_EQ) {
    MakeAccess(curList, pn->child_at(0), OP_LDST | OP_LOAD);
    curList->newNode<ANOpCode>(op_push);
  }

  // Compile the value to be assigned.
  CompileExpr(curList, pn->child_at(1));

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

  MakeAccess(curList, pn->child_at(0), OP_LDST | OP_STORE);
}

static void MakeReturn(AOpList* curList, PNode* pn) {
  // Compile code for a return.

  // If there was an argument to the return, compile it.
  if (pn->first_child()) CompileExpr(curList, pn->first_child());

  // Put out the return opcode.
  curList->newNode<ANOpCode>(op_ret);
}

void MakeBranch(AOpList* curList, uint8_t theCode, ANLabel* target) {
  // Compile code for a branch.  The type of branch is in 'theCode', the
  // destination is 'bn'.  If the the destination is not yet defined,
  // 'dest' will point to a the symbol of the destination.

  ANBranch* an = curList->newNode<ANBranch>(theCode);
  an->target = target;
}

void MakeBranch(AOpList* curList, uint8_t theCode,
                ForwardReference<ANLabel*>* dest) {
  // Compile code for a branch.  The type of branch is in 'theCode', the
  // destination is 'bn'.  If the the destination is not yet defined,
  // 'dest' will point to a the symbol of the destination.

  ANBranch* an = curList->newNode<ANBranch>(theCode);
  dest->RegisterCallback([an](ANLabel* target) { an->target = target; });
}

static void MakeComp(AOpList* curList, PNode* pn) {
  // Compile a comparison expression.  These expressions are nary expressions
  // with an early out -- the moment the truth value of the expression is
  // known, we stop evaluating the expression.

  // Get the comparison operator.
  uint32_t op = pn->val;

  if (op == N_OR)
    // Handle special case of '||'.
    MakeOr(curList, pn->children);
  else if (op == N_AND)
    // Handle special case of '&&'.
    MakeAnd(curList, pn->children);
  else {
    // Point to the first argument and set up an empty need list (which
    // will be used to keep track of those nodes which need the address
    // of the end of the expression for the early out).
    ForwardReference<ANLabel*> earlyOut;

    // Compile the first two operands and do the test.
    CompileExpr(curList, pn->child_at(0));
    curList->newNode<ANOpCode>(op_push);
    CompileExpr(curList, pn->child_at(1));
    MakeCompOp(curList, op);

    // If there are no more operands, we're done.  Otherwise we've got
    // to bail out of the test if it is already false or continue if it
    // is true so far.
    for (auto const& node : pn->rest_at(2)) {
      // Early out if false.
      MakeBranch(curList, op_bnt, &earlyOut);

      // Push the previous accumulator value on the stack in
      // order to continue the comparison.
      curList->newNode<ANOpCode>(op_pprev);

      // Compile the next argument and test it.
      CompileExpr(curList, node.get());
      MakeCompOp(curList, op);
    }

    // Set the target for any branches to the end of the expression
    MakeLabel(curList, &earlyOut);
  }
}

static void MakeAnd(AOpList* curList, PNode::ChildSpan args) {
  // Compile code for the '&&' operator.

  ForwardReference<ANLabel*> earlyOut;

  CompileExpr(curList, args[0].get());

  for (auto const& arg : args.subspan(1)) {
    // Make a branch for an early out if the expression is false.
    MakeBranch(curList, op_bnt, &earlyOut);
    // Compile an argument.
    CompileExpr(curList, arg.get());
  }

  // Set the target for any early-out branches.
  MakeLabel(curList, &earlyOut);
}

static void MakeOr(AOpList* curList, PNode::ChildSpan args) {
  // Compile code for the '||' operator.

  ForwardReference<ANLabel*> earlyOut;

  CompileExpr(curList, args[0].get());

  for (auto const& arg : args.subspan(1)) {
    // Make a branch for an early out if the expression is true.
    MakeBranch(curList, op_bt, &earlyOut);
    // Compile code for an argument.
    CompileExpr(curList, arg.get());
  }

  // Make a target for the early-out branches.
  MakeLabel(curList, &earlyOut);
}

static void MakeCompOp(AOpList* curList, int op) {
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

static void MakeIf(AOpList* curList, PNode* pn) {
  // Compile code for an 'if' statement.

  // Compile the conditional expression
  CompileExpr(curList, pn->child_at(0));

  // Branch to the else code (if there is any) if the expression is false.
  ForwardReference<ANLabel*> elseLabel;
  MakeBranch(curList, op_bnt, &elseLabel);

  // Compile the code to be executed if expression was true.
  if (pn->child_at(1)) CompileExpr(curList, pn->child_at(1));

  // If there is no 'else' code, we're done -- backpatch the branch.
  // Otherwise, jump around the else code, backpatch the jump to the
  // else code, compile the else code, and backpatch the jump around
  // the else code.
  if (!pn->child_at(2))
    MakeLabel(curList, &elseLabel);

  else {
    ForwardReference<ANLabel*> doneLabel;
    MakeBranch(curList, op_jmp, &doneLabel);
    MakeLabel(curList, &elseLabel);
    CompileExpr(curList, pn->child_at(2));
    MakeLabel(curList, &doneLabel);
  }
}

static void MakeCond(AOpList* curList, PNode* pn) {
  // Compile code for a 'cond' expression.

  ForwardReference<ANLabel*> done;
  bool elseSeen = false;

  // Children alternate between conditions and body.
  // Bodies are always an instance of ELIST, which is used to detect if there
  // is any body for each clause.

  std::size_t i = 0;
  while (i < pn->children.size()) {
    ForwardReference<ANLabel*> next;
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
      CompileExpr(curList, condition);

      if (atEnd && !body) {
        //	if we're at the end, just break out.
        break;
      }

      //	if we're on the last test and it fails, exit
      if (body && atEnd) {
        MakeBranch(curList, op_bnt, &done);
        //	if we're on an interior test and it fails, go to next test
      } else {
        MakeBranch(curList, op_bnt, &next);
      }

    } else if (elseSeen) {
      Error("Multiple else clauses");
    } else {
      elseSeen = true;
    }

    // Compile the statements to be executed if a condition was
    // satisfied.
    if (body) {
      CompileExpr(curList, body);
    }

    // If we're at the end of the cond clause, we're done.  Otherwise
    // make a jump to the end of the cond clause and compile a
    // destination for the jump which branched around the code
    // just compiled.
    if (!atEnd) {
      MakeBranch(curList, op_jmp, &done);
      MakeLabel(curList, &next);
    }
  }

  // Make a destination for jumps to the end of the cond clause.
  MakeLabel(curList, &done);
}

static void MakeSwitch(AOpList* curList, PNode* pn) {
  // Compile code for the 'switch' statement.

  ForwardReference<ANLabel*> done;
  bool elseSeen = false;

  PNode::ChildSpan children = pn->children;
  auto* value = children[0].get();
  auto cases = children.subspan(1);

  // Compile the expression to be switched on and put the value on
  // the stack.
  CompileExpr(curList, value);
  curList->newNode<ANOpCode>(op_push);

  std::size_t i = 0;
  while (i < cases.size()) {
    ForwardReference<ANLabel*> next;
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
      CompileExpr(curList, caseClause);

      // Test for equality.
      curList->newNode<ANOpCode>(op_eq);

      //	if we're at the end, just fall through
      if (atEnd && !body) {
        break;
      }

      //	if we're on the last test and it fails, exit
      if (atEnd && body) {
        MakeBranch(curList, op_bnt, &done);

      } else {
        //	if we're on an interior test and it fails, go to next test
        MakeBranch(curList, op_bnt, &next);
      }

    } else if (elseSeen) {
      Error("Multiple else clauses");
    } else {
      elseSeen = true;
    }

    // Compile the statements to be executed if a switch matched.
    if (body) {
      CompileExpr(curList, body);
    }

    // If we're at the end of the switch expression, we're done.
    // Otherwise, make a jump to the end of the expression, then
    // make a target for the branch around the previous code.
    if (!atEnd) {
      MakeBranch(curList, op_jmp, &done);
      MakeLabel(curList, &next);
    }
  }

  // Compile a target for jumps to the end of the switch expression.
  MakeLabel(curList, &done);

  // Take the switch value off the stack.
  curList->newNode<ANOpCode>(op_toss);
}

static void MakeIncDec(AOpList* curList, PNode* pn) {
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
  MakeAccess(curList, pn->first_child(), (uint8_t)theCode);
}

static void MakeProc(AList* curList, PNode* pn) {
  // Compile code for an entire procedure.

  // Make a procedure node and point to the symbol for the procedure
  // (for listing purposes).
  ANCodeBlk* an = pn->type == PN_PROC
                      ? (ANCodeBlk*)curList->newNode<ANProcCode>(
                            std::string(pn->sym->name()))
                      : (ANCodeBlk*)curList->newNode<ANMethCode>(
                            std::string(pn->sym->name()));

  pn->sym->type = (sym_t)(pn->type == PN_PROC ? S_PROC : S_SELECT);

  // If any nodes already compiled have this procedure as a target,
  // they will be on a list hanging off the procedure's symbol table
  // entry (in the 'ref' property) (compiled by the first reference to the
  // procedure).  Let all these nodes know where this one is.
  pn->sym->setLoc(an);

  //	procedures and methods get special treatment:  the line number
  //	and file name are set here
  if (gConfig->includeDebugInfo) {
    an->code.newNode<ANLineNum>(pn->lineNum);
    gLastLineNum = pn->lineNum;
  }

  // If there are to be any temporary variables, add a link node to
  // create them.
  if (pn->val) an->code.newNode<ANOpUnsign>(op_link, pn->val);

  // Compile code for the procedure followed by a return.
  if (pn->child_at(0)) CompileExpr(&an->code, pn->child_at(0));

  if (gConfig->includeDebugInfo) {
    an->code.newNode<ANLineNum>(gInputState.GetTopLevelLineNum());
  }
  an->code.newNode<ANOpCode>(op_ret);
}

void MakeDispatch(int maxEntry) {
  // Compile the dispatch table which goes at the start of this script.

  // Now cycle through the publicly declared procedures/objects,
  // creating asmNodes for a table of their offsets.
  gNumDispTblEntries->value = maxEntry + 1;
  for (int i = 0; i <= maxEntry; ++i) {
    ANDispatch* an = gDispTbl->entries.newNode<ANDispatch>();
    auto* sym = FindPublic(i);
    if (sym) {
      an->name = std::string(sym->name());
      sym->forwardRef.RegisterCallback(
          [an](ANode* target) { an->target = target; });
    }
  }
}

void MakeObject(Object* theObj) {
  ANOfsProp* pDict = nullptr;
  ANOfsProp* mDict = nullptr;

  {
    // Create the object ID node.
    ANObject* obj =
        gSc->heapList->getList()->newNodeBefore<ANObject>(nullptr, theObj);
    theObj->an = obj;

    // Create the table of properties.
    ANTable* props =
        gSc->heapList->getList()->newNodeBefore<ANTable>(nullptr, "properties");

    {
      for (auto* sp : theObj->selectors())
        if (IsProperty(sp)) {
          switch (sp->tag) {
            case T_PROP:
              gSc->heapList->getList()->newNode<ANIntProp>(
                  std::string(sp->sym->name()), sp->val);
              break;

            case T_TEXT:
              gSc->heapList->getList()->newNode<ANTextProp>(
                  std::string(sp->sym->name()), sp->val);
              break;

            case T_PROPDICT:
              pDict = gSc->heapList->getList()->newNode<ANOfsProp>(
                  std::string(sp->sym->name()));
              break;

            case T_METHDICT:
              mDict = gSc->heapList->getList()->newNode<ANOfsProp>(
                  std::string(sp->sym->name()));
              break;
          }
        }
    }

    // If any nodes already compiled have this object as a target, they
    // will be on a list hanging off the object's symbol table entry.
    // Let all nodes know where this one is.
    theObj->sym->setLoc(props);
  }

  // The rest of the object goes into hunk, as it never changes.
  gSc->hunkList->getList()->newNodeBefore<ANObject>(gCodeStart, theObj);

  // If this a class, add the property dictionary.
  ANObjTable* propDict = gSc->hunkList->getList()->newNodeBefore<ANObjTable>(
      gCodeStart, "property dictionary");
  {
    if (theObj->num != OBJECTNUM) {
      for (auto* sp : theObj->selectors())
        if (IsProperty(sp)) propDict->entries.newNode<ANWord>(sp->sym->val());
    }
  }
  if (pDict) pDict->target = propDict;

  ANObjTable* methDict = gSc->hunkList->getList()->newNodeBefore<ANObjTable>(
      gCodeStart, "method dictionary");
  {
    ANWord* numMeth = methDict->entries.newNode<ANWord>((short)0);
    for (auto* sp : theObj->selectors())
      if (sp->tag == T_LOCAL) {
        methDict->entries.newNode<ANWord>(sp->sym->val());
        methDict->entries.newNode<ANMethod>(std::string(sp->sym->name()),
                                            (ANMethCode*)sp->an);
        sp->sym->forwardRef.Clear();
        ++numMeth->value;
      }
  }
  if (mDict) mDict->target = methDict;
}

void MakeText() {
  // Add text strings to the assembled code.

  // terminate the object portion of the heap with a null word
  gSc->heapList->newNode<ANWord>();
  for (Text* tp : gText.items()) gSc->heapList->newNode<ANText>(tp);
}

void MakeLabel(AOpList* curList, ForwardReference<ANLabel*>* dest) {
  auto* label = curList->newNode<ANLabel>();
  dest->Resolve(label);
}
