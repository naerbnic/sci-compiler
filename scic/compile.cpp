//	compile.cpp
// 	compile the object code list for a parse tree

#include "scic/compile.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <utility>

#include "scic/codegen/alist.hpp"
#include "scic/codegen/anode.hpp"
#include "scic/codegen/anode_impls.hpp"
#include "scic/codegen/code_generator.hpp"
#include "scic/config.hpp"
#include "scic/error.hpp"
#include "scic/global_compiler.hpp"
#include "scic/input.hpp"
#include "scic/loop.hpp"
#include "scic/object.hpp"
#include "scic/opcodes.hpp"
#include "scic/parse_context.hpp"
#include "scic/pnode.hpp"
#include "scic/public.hpp"
#include "scic/symtypes.hpp"
#include "util/types/forward_ref.hpp"

static void MakeAccess(FunctionBuilder* builder, PNode*, uint8_t);
static void MakeImmediate(FunctionBuilder* builder, int);
static void MakeString(FunctionBuilder* builder, PNode*);
static void MakeCall(FunctionBuilder* builder, PNode* pn);
static void MakeClassID(FunctionBuilder* builder, PNode*);
static void MakeObjID(FunctionBuilder* builder, PNode*);
static void MakeSend(FunctionBuilder* builder, PNode*);
static int MakeMessage(FunctionBuilder* builder, PNode::ChildSpan children);
static void MakeProc(PNode*);
static int MakeArgs(FunctionBuilder* builder, PNode::ChildSpan children);
static void MakeUnary(FunctionBuilder* builder, PNode*);
static void MakeBinary(FunctionBuilder* builder, PNode*);
static void MakeNary(FunctionBuilder* builder, PNode*);
static void MakeAssign(FunctionBuilder* builder, PNode*);
static void MakeReturn(FunctionBuilder* builder, PNode*);
static void MakeComp(FunctionBuilder* builder, PNode*);
static void MakeAnd(FunctionBuilder* builder, PNode::ChildSpan);
static void MakeOr(FunctionBuilder* builder, PNode::ChildSpan);
static void MakeIncDec(FunctionBuilder* builder, PNode*);
static void MakeCompOp(FunctionBuilder* builder, int);
static void MakeIf(FunctionBuilder* builder, PNode*);
static void MakeCond(FunctionBuilder* builder, PNode*);
static void MakeSwitch(FunctionBuilder* builder, PNode*);

// This global is only used in this module
static int lastLineNum;

void CompileProc(PNode* pn) {
  // Recursively compile code for a given node.

  switch (pn->type) {
    case PN_PROC:
    case PN_METHOD:
      MakeProc(pn);
      break;

    // Do nothing for unhandled node types.
    default:
      Error("Internal error: unhandled node type in Compile(): %d", pn->type);
      break;
  }
}

void CompileExpr(FunctionBuilder* builder, PNode* pn) {
  // Recursively compile code for a given node.

  if (gConfig->includeDebugInfo && pn->type != PN_PROC &&
      pn->type != PN_METHOD && pn->lineNum > lastLineNum) {
    builder->GetOpList()->newNode<ANLineNum>(pn->lineNum);
    lastLineNum = pn->lineNum;
  }

  switch (pn->type) {
    case PN_ELIST:
      // An expression list.  Compile code for each expression
      // in the list.
      for (auto const& child : pn->children) CompileExpr(builder, child.get());
      break;

    case PN_EXPR:
      // Compile the expression.
      CompileExpr(builder, pn->first_child());
      break;

    case PN_ASSIGN:
      MakeAssign(builder, pn);
      break;

    case PN_SELECT:
      MakeImmediate(builder, pn->val);
      break;

    case PN_NUM:
      MakeImmediate(builder, pn->val);
      break;

    case PN_STRING:
      MakeString(builder, pn);
      break;

    case PN_GLOBAL:
    case PN_LOCAL:
    case PN_TMP:
    case PN_PARM:
    case PN_INDEX:
    case PN_PROP:
      // Compile code to load the accumulator from a variable.
      MakeAccess(builder, pn, OP_LDST | OP_LOAD);
      break;

    case PN_ADDROF:
      MakeAccess(builder, pn->first_child(), op_lea);
      break;

    case PN_CLASS:
      MakeClassID(builder, pn);
      break;

    case PN_OBJ:
      MakeObjID(builder, pn);
      break;

    case PN_EXTERN:
    case PN_CALL:
      MakeCall(builder, pn);
      break;

    case PN_SEND:
      MakeSend(builder, pn);
      break;

    case PN_UNARY:
      MakeUnary(builder, pn);
      break;

    case PN_BINARY:
      MakeBinary(builder, pn);
      break;

    case PN_NARY:
      MakeNary(builder, pn);
      break;

    case PN_COMP:
      MakeComp(builder, pn);
      break;

    case PN_RETURN:
      MakeReturn(builder, pn);
      break;

    case PN_IF:
      MakeIf(builder, pn);
      break;

    case PN_COND:
      MakeCond(builder, pn);
      break;

    case PN_SWITCH:
    case PN_SWITCHTO:
      MakeSwitch(builder, pn);
      break;

    case PN_INCDEC:
      MakeIncDec(builder, pn);
      break;

      // The following routines are in 'loop.cpp'.
    case PN_WHILE:
      MakeWhile(builder, pn);
      break;

    case PN_REPEAT:
      MakeRepeat(builder, pn);
      break;

    case PN_FOR:
      MakeFor(builder, pn);
      break;

    case PN_BREAK:
      MakeBreak(builder, pn);
      break;

    case PN_BREAKIF:
      MakeBreakIf(builder, pn);
      break;

    case PN_CONT:
      MakeContinue(builder, pn);
      break;

    case PN_CONTIF:
      MakeContIf(builder, pn);
      break;

    // Do nothing for unhandled node types.
    default:
      Error("Internal error: unhandled node type in Compile(): %d", pn->type);
      break;
  }
}

static void MakeAccess(FunctionBuilder* builder, PNode* pn, uint8_t theCode) {
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
      builder->GetOpList()->newNode<ANOpCode>(
          op_push);  // push the value to store on the stack
    CompileExpr(builder, pn->children[1].get());  // compile index value
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
    an = builder->GetOpList()->newNode<ANEffctAddr>(theCode, theAddr, accType);

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
    an = builder->GetOpList()->newNode<ANVarAccess>(theCode, theAddr);
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

static void MakeImmediate(FunctionBuilder* builder, int val) {
  builder->GetOpList()->newNode<ANOpSign>(op_loadi, val);
}

static void MakeString(FunctionBuilder* builder, PNode* pn) {
  builder->GetOpList()->newNode<ANOpOfs>(pn->str);
}

static void MakeCall(FunctionBuilder* builder, PNode* pn) {
  // Compile a call to a procedure, the kernel, etc.

  // Push the number of arguments on the stack (we don't know the
  // number yet).
  ANOpUnsign* an = builder->GetOpList()->newNode<ANOpUnsign>(op_pushi, 0);

  // Compile the arguments.
  uint32_t numArgs = MakeArgs(builder, pn->children);

  // Put the number of arguments in its asmNode.
  an->value = numArgs;

  // Compile the call.
  Symbol* sym = pn->sym;
  if (pn->type == PN_CALL) {
    ANCall* call =
        builder->GetOpList()->newNode<ANCall>(std::string(sym->name()));

    // If the destination procedure has not yet been defined, add
    // this node to the list of those waiting for its definition.
    sym->forwardRef.RegisterCallback(
        [call](ANode* target) { call->target = target; });
    call->numArgs = 2 * numArgs;

  } else {
    Public* pub = sym->ext();
    ANOpExtern* extCall = builder->GetOpList()->newNode<ANOpExtern>(
        std::string(sym->name()), pub->script, pub->entry);
    extCall->numArgs = 2 * numArgs;
  }
}

static void MakeClassID(FunctionBuilder* builder, PNode* pn) {
  // Compile a class ID.

  ANOpUnsign* an =
      builder->GetOpList()->newNode<ANOpUnsign>(op_class, pn->sym->obj()->num);
  an->name = std::string(pn->sym->name());
}

static void MakeObjID(FunctionBuilder* builder, PNode* pn) {
  // Compile an object ID.

  if (pn->sym->hasVal(OBJ_SELF))
    builder->GetOpList()->newNode<ANOpCode>(op_selfID);

  else {
    Symbol* sym = pn->sym;
    if (!sym) {
      Error("Undefined object from line %u: %s", sym->lineNum, sym->name());
      return;
    }
    ANObjID* an = builder->GetOpList()->newNode<ANObjID>(
        sym->lineNum, std::string(sym->name()));

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

static void MakeSend(FunctionBuilder* builder, PNode* pn) {
  // Compile code for sending to an object through a handle stored in a
  // variable.

  // Get pointer to object node (an expression)
  PNode* on = pn->children[0].get();

  // Compile the messages to the object.
  int numArgs = 0;
  for (auto const& msg :
       std::ranges::subrange(pn->children.begin() + 1, pn->children.end())) {
    numArgs += MakeMessage(builder, msg->children);
  }

  // Add the appropriate send.
  ANSend* an;
  if (on->type == PN_OBJ && on->val == (int)OBJ_SELF)
    an = builder->GetOpList()->newNode<ANSend>(op_self);
  else if (on->type == PN_SUPER)
    an = builder->GetOpList()->newNode<ANSuper>(std::string(on->sym->name()),
                                                on->val);
  else {
    CompileExpr(builder, on);  // compile the object/class id
    an = builder->GetOpList()->newNode<ANSend>(op_send);
  }

  an->numArgs = 2 * numArgs;
}

static int MakeMessage(FunctionBuilder* builder, PNode::ChildSpan theMsg) {
  // Compile code to push a message on the stack.

  // Compile the selector.
  CompileExpr(builder, theMsg.front().get());
  builder->GetOpList()->newNode<ANOpCode>(op_push);

  // Push the number of arguments on the stack (we don't know the
  // number yet).
  ANOpUnsign* an =
      builder->GetOpList()->newNode<ANOpUnsign>(op_pushi, (uint32_t)-1);

  // Compile the arguments to the message and fix up the number
  // of arguments to the message.
  an->value = MakeArgs(builder, theMsg.subspan(1));

  return an->value + 2;
}

static int MakeArgs(FunctionBuilder* builder, PNode::ChildSpan args) {
  // Compile code to push the arguments to a call on the stack.
  // Return the number of arguments.

  int numArgs = 0;

  for (auto const& arg : args) {
    if (arg->type == PN_REST)
      builder->GetOpList()->newNode<ANOpUnsign>(op_rest | OP_BYTE, arg->val);
    else {
      CompileExpr(builder, arg.get());
      builder->GetOpList()->newNode<ANOpCode>(op_push);
      ++numArgs;
    }
  }

  return numArgs;
}

static void MakeUnary(FunctionBuilder* builder, PNode* pn) {
  // Compile code for unary operators.

  // Do the argument to the operator.
  CompileExpr(builder, pn->first_child());

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
  builder->GetOpList()->newNode<ANOpCode>(theCode);
}

static void MakeBinary(FunctionBuilder* builder, PNode* pn) {
  // Compile code for a binary operator.

  // Compile the arguments, putting the first on the stack.
  CompileExpr(builder, pn->child_at(0));
  builder->GetOpList()->newNode<ANOpCode>(op_push);
  CompileExpr(builder, pn->child_at(1));

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
  builder->GetOpList()->newNode<ANOpCode>(theCode);
}

static void MakeNary(FunctionBuilder* builder, PNode* pn) {
  // Compile code for an nary operator (one with any number of arguments).

  // Compile the first argument and push its value on the stack.
  CompileExpr(builder, pn->child_at(0));

  for (auto const& arg : pn->rest()) {
    // Push the previous argument on the stack for combining with the
    // next argument.
    builder->GetOpList()->newNode<ANOpCode>(op_push);
    // Compile the next argument.
    CompileExpr(builder, arg.get());

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
    builder->GetOpList()->newNode<ANOpCode>(theCode);
  }
}

static void MakeAssign(FunctionBuilder* builder, PNode* pn) {
  // If this is an arithmetic-op assignment, put the value of the
  // target variable on the stack for the operation.
  if (pn->val != A_EQ) {
    MakeAccess(builder, pn->child_at(0), OP_LDST | OP_LOAD);
    builder->GetOpList()->newNode<ANOpCode>(op_push);
  }

  // Compile the value to be assigned.
  CompileExpr(builder, pn->child_at(1));

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
    builder->GetOpList()->newNode<ANOpCode>(theCode);
  }

  MakeAccess(builder, pn->child_at(0), OP_LDST | OP_STORE);
}

static void MakeReturn(FunctionBuilder* builder, PNode* pn) {
  // Compile code for a return.

  // If there was an argument to the return, compile it.
  if (pn->first_child()) CompileExpr(builder, pn->first_child());

  // Put out the return opcode.
  builder->GetOpList()->newNode<ANOpCode>(op_ret);
}

void MakeBranch(FunctionBuilder* builder, uint8_t theCode, ANLabel* target) {
  // Compile code for a branch.  The type of branch is in 'theCode', the
  // destination is 'bn'.  If the the destination is not yet defined,
  // 'dest' will point to a the symbol of the destination.

  ANBranch* an = builder->GetOpList()->newNode<ANBranch>(theCode);
  an->target = target;
}

void MakeBranch(FunctionBuilder* builder, uint8_t theCode,
                ForwardRef<ANLabel*>* dest) {
  // Compile code for a branch.  The type of branch is in 'theCode', the
  // destination is 'bn'.  If the the destination is not yet defined,
  // 'dest' will point to a the symbol of the destination.

  ANBranch* an = builder->GetOpList()->newNode<ANBranch>(theCode);
  dest->RegisterCallback([an](ANLabel* target) { an->target = target; });
}

static void MakeComp(FunctionBuilder* builder, PNode* pn) {
  // Compile a comparison expression.  These expressions are nary expressions
  // with an early out -- the moment the truth value of the expression is
  // known, we stop evaluating the expression.

  // Get the comparison operator.
  uint32_t op = pn->val;

  if (op == N_OR)
    // Handle special case of '||'.
    MakeOr(builder, pn->children);
  else if (op == N_AND)
    // Handle special case of '&&'.
    MakeAnd(builder, pn->children);
  else {
    // Point to the first argument and set up an empty need list (which
    // will be used to keep track of those nodes which need the address
    // of the end of the expression for the early out).
    ForwardRef<ANLabel*> earlyOut;

    // Compile the first two operands and do the test.
    CompileExpr(builder, pn->child_at(0));
    builder->GetOpList()->newNode<ANOpCode>(op_push);
    CompileExpr(builder, pn->child_at(1));
    MakeCompOp(builder, op);

    // If there are no more operands, we're done.  Otherwise we've got
    // to bail out of the test if it is already false or continue if it
    // is true so far.
    for (auto const& node : pn->rest_at(2)) {
      // Early out if false.
      MakeBranch(builder, op_bnt, &earlyOut);

      // Push the previous accumulator value on the stack in
      // order to continue the comparison.
      builder->GetOpList()->newNode<ANOpCode>(op_pprev);

      // Compile the next argument and test it.
      CompileExpr(builder, node.get());
      MakeCompOp(builder, op);
    }

    // Set the target for any branches to the end of the expression
    MakeLabel(builder, &earlyOut);
  }
}

static void MakeAnd(FunctionBuilder* builder, PNode::ChildSpan args) {
  // Compile code for the '&&' operator.

  ForwardRef<ANLabel*> earlyOut;

  CompileExpr(builder, args[0].get());

  for (auto const& arg : args.subspan(1)) {
    // Make a branch for an early out if the expression is false.
    MakeBranch(builder, op_bnt, &earlyOut);
    // Compile an argument.
    CompileExpr(builder, arg.get());
  }

  // Set the target for any early-out branches.
  MakeLabel(builder, &earlyOut);
}

static void MakeOr(FunctionBuilder* builder, PNode::ChildSpan args) {
  // Compile code for the '||' operator.

  ForwardRef<ANLabel*> earlyOut;

  CompileExpr(builder, args[0].get());

  for (auto const& arg : args.subspan(1)) {
    // Make a branch for an early out if the expression is true.
    MakeBranch(builder, op_bt, &earlyOut);
    // Compile code for an argument.
    CompileExpr(builder, arg.get());
  }

  // Make a target for the early-out branches.
  MakeLabel(builder, &earlyOut);
}

static void MakeCompOp(FunctionBuilder* builder, int op) {
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

  builder->GetOpList()->newNode<ANOpCode>(theCode);
}

static void MakeIf(FunctionBuilder* builder, PNode* pn) {
  // Compile code for an 'if' statement.

  // Compile the conditional expression
  CompileExpr(builder, pn->child_at(0));

  // Branch to the else code (if there is any) if the expression is false.
  ForwardRef<ANLabel*> elseLabel;
  MakeBranch(builder, op_bnt, &elseLabel);

  // Compile the code to be executed if expression was true.
  if (pn->child_at(1)) CompileExpr(builder, pn->child_at(1));

  // If there is no 'else' code, we're done -- backpatch the branch.
  // Otherwise, jump around the else code, backpatch the jump to the
  // else code, compile the else code, and backpatch the jump around
  // the else code.
  if (!pn->child_at(2))
    MakeLabel(builder, &elseLabel);

  else {
    ForwardRef<ANLabel*> doneLabel;
    MakeBranch(builder, op_jmp, &doneLabel);
    MakeLabel(builder, &elseLabel);
    CompileExpr(builder, pn->child_at(2));
    MakeLabel(builder, &doneLabel);
  }
}

static void MakeCond(FunctionBuilder* builder, PNode* pn) {
  // Compile code for a 'cond' expression.

  ForwardRef<ANLabel*> done;
  bool elseSeen = false;

  // Children alternate between conditions and body.
  // Bodies are always an instance of ELIST, which is used to detect if there
  // is any body for each clause.

  std::size_t i = 0;
  while (i < pn->children.size()) {
    ForwardRef<ANLabel*> next;
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
      CompileExpr(builder, condition);

      if (atEnd && !body) {
        //	if we're at the end, just break out.
        break;
      }

      //	if we're on the last test and it fails, exit
      if (body && atEnd) {
        MakeBranch(builder, op_bnt, &done);
        //	if we're on an interior test and it fails, go to next test
      } else {
        MakeBranch(builder, op_bnt, &next);
      }

    } else if (elseSeen) {
      Error("Multiple else clauses");
    } else {
      elseSeen = true;
    }

    // Compile the statements to be executed if a condition was
    // satisfied.
    if (body) {
      CompileExpr(builder, body);
    }

    // If we're at the end of the cond clause, we're done.  Otherwise
    // make a jump to the end of the cond clause and compile a
    // destination for the jump which branched around the code
    // just compiled.
    if (!atEnd) {
      MakeBranch(builder, op_jmp, &done);
      MakeLabel(builder, &next);
    }
  }

  // Make a destination for jumps to the end of the cond clause.
  MakeLabel(builder, &done);
}

static void MakeSwitch(FunctionBuilder* builder, PNode* pn) {
  // Compile code for the 'switch' statement.

  ForwardRef<ANLabel*> done;
  bool elseSeen = false;

  PNode::ChildSpan children = pn->children;
  auto* value = children[0].get();
  auto cases = children.subspan(1);

  // Compile the expression to be switched on and put the value on
  // the stack.
  CompileExpr(builder, value);
  builder->GetOpList()->newNode<ANOpCode>(op_push);

  std::size_t i = 0;
  while (i < cases.size()) {
    ForwardRef<ANLabel*> next;
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
      builder->GetOpList()->newNode<ANOpCode>(op_dup);

      // Compile the test value.
      CompileExpr(builder, caseClause);

      // Test for equality.
      builder->GetOpList()->newNode<ANOpCode>(op_eq);

      //	if we're at the end, just fall through
      if (atEnd && !body) {
        break;
      }

      //	if we're on the last test and it fails, exit
      if (atEnd && body) {
        MakeBranch(builder, op_bnt, &done);

      } else {
        //	if we're on an interior test and it fails, go to next test
        MakeBranch(builder, op_bnt, &next);
      }

    } else if (elseSeen) {
      Error("Multiple else clauses");
    } else {
      elseSeen = true;
    }

    // Compile the statements to be executed if a switch matched.
    if (body) {
      CompileExpr(builder, body);
    }

    // If we're at the end of the switch expression, we're done.
    // Otherwise, make a jump to the end of the expression, then
    // make a target for the branch around the previous code.
    if (!atEnd) {
      MakeBranch(builder, op_jmp, &done);
      MakeLabel(builder, &next);
    }
  }

  // Compile a target for jumps to the end of the switch expression.
  MakeLabel(builder, &done);

  // Take the switch value off the stack.
  builder->GetOpList()->newNode<ANOpCode>(op_toss);
}

static void MakeIncDec(FunctionBuilder* builder, PNode* pn) {
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
  MakeAccess(builder, pn->first_child(), (uint8_t)theCode);
}

static void MakeProc(PNode* pn) {
  // Compile code for an entire procedure.

  auto procName =
      pn->type == PN_PROC
          ? FuncName::Make<ProcedureName>(std::string(pn->sym->name()))
          : FuncName::Make<MethodName>(gCurObj->name,
                                       std::string(pn->sym->name()));

  std::optional<std::size_t> lineNum;
  if (gConfig->includeDebugInfo) {
    lineNum = pn->lineNum;
    lastLineNum = pn->lineNum;
  }

  // Make a procedure node and point to the symbol for the procedure
  // (for listing purposes).
  auto func_builder =
      gSc->CreateFunction(std::move(procName), lineNum, /*numTemps=*/pn->val);

  pn->sym->type = (sym_t)(pn->type == PN_PROC ? S_PROC : S_SELECT);

  // If any nodes already compiled have this procedure as a target,
  // they will be on a list hanging off the procedure's symbol table
  // entry (in the 'ref' property) (compiled by the first reference to the
  // procedure).  Let all these nodes know where this one is.
  pn->sym->setLoc(func_builder->GetNode());

  // Compile code for the procedure followed by a return.
  if (pn->child_at(0)) CompileExpr(func_builder.get(), pn->child_at(0));

  if (gConfig->includeDebugInfo) {
    func_builder->GetOpList()->newNode<ANLineNum>(
        gInputState.GetTopLevelLineNum());
  }
  func_builder->GetOpList()->newNode<ANOpCode>(op_ret);
}

void MakeObject(Object* theObj) {
  auto objCodegen = theObj->num == OBJECTNUM ? gSc->CreateObject(theObj->name)
                                             : gSc->CreateClass(theObj->name);

  {
    theObj->an = objCodegen->GetObjNode();

    {
      for (auto* sp : theObj->selectors())
        if (IsProperty(sp)) {
          switch (sp->tag) {
            case T_PROP:
            case T_TEXT:
              objCodegen->AppendProperty(std::string(sp->sym->name()),
                                         sp->sym->val(), *sp->val);
              break;

            case T_PROPDICT:
              objCodegen->AppendPropTableProperty(std::string(sp->sym->name()),
                                                  sp->sym->val());
              break;

            case T_METHDICT:
              objCodegen->AppendMethodTableProperty(
                  std::string(sp->sym->name()), sp->sym->val());
              break;
          }
        }
    }

    // If any nodes already compiled have this object as a target, they
    // will be on a list hanging off the object's symbol table entry.
    // Let all nodes know where this one is.
    theObj->sym->setLoc(objCodegen->GetObjNode());
  }

  {
    for (auto* sp : theObj->selectors())
      if (sp->tag == T_LOCAL) {
        objCodegen->AppendMethod(std::string(sp->sym->name()), sp->sym->val(),
                                 (ANCodeBlk*)sp->an);
        sp->sym->forwardRef.Clear();
      }
  }
}

void MakeLabel(FunctionBuilder* builder, ForwardRef<ANLabel*>* dest) {
  auto* label = builder->GetOpList()->newNode<ANLabel>();
  dest->Resolve(label);
}
