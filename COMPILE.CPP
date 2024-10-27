//	compile.cpp
// 	compile the object code list for a parse tree

#include "sol.hpp"

#include	"sc.hpp"

#include	"anode.hpp"
#include	"asm.hpp"
#include	"compile.hpp"
#include	"define.hpp"
#include	"error.hpp"
#include	"input.hpp"
#include	"parse.hpp"
#include	"opcodes.hpp"
#include	"object.hpp"
#include	"text.hpp"

static void		MakeAccess(PNode*, ubyte);
static void		MakeImmediate(int);
static void		MakeString(PNode*);
static void		MakeCall(PNode* pn);
static void		MakeClassID(PNode*);
static void		MakeObjID(PNode*);
static void		MakeSend(PNode*);
static int		MakeMessage(PNode*);
static void		MakeProc(PNode*);
static Bool		MakeArgs(PNode*);
static void		MakeUnary(PNode*);
static void		MakeBinary(PNode*);
static void		MakeNary(PNode*);
static void		MakeAssign(PNode*);
static void		MakeReturn(PNode*);
static void		MakeComp(PNode*);
static void		MakeAnd(PNode*);
static void		MakeOr(PNode*);
static void		MakeIncDec(PNode*);
static void		MakeCompOp(Bool);
static void		MakeIf(PNode*);
static void		MakeCond(PNode*);
static void		MakeSwitch(PNode*);

void
CompileCode(PNode* pn)
{
	// Called pointing to the root of a parsed procedure.  Compile code for
	// the procedure then delete its parse tree.

	Compile(pn);
	delete pn;
}

void
Compile(PNode* pn)
{
	// Recursively compile code for a given node.

	if (includeDebugInfo &&
		 pn->type != PN_PROC && pn->type != PN_METHOD &&
		 pn->lineNum > lastLineNum) {
		New ANLineNum(pn->lineNum);
		lastLineNum = pn->lineNum;
	}

	switch (pn->type) {
		case PN_ELIST:
			// An expression list.  Compile code for each expression
			// in the list.
			for (pn = pn->child; pn; pn = pn->next)
				Compile(pn);
			break;

		case PN_EXPR:
			// Compile the expression.
			Compile(pn->child);
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
		}
}

static void
MakeAccess(PNode* pn, ubyte theCode)
{
	// Compile code to access the variable indicated by pn.  Access
	// is OP_STORE or OP_LOAD

	ANVarAccess*	an;
	PNode*			child;
	uword				theAddr;
	pn_t				varType;

	// Check for indexing and compile the index if necessary.
	Bool indexed = pn->type == PN_INDEX;
	if (indexed) {
		child = pn->child;
		if (theCode == (OP_LDST | OP_STORE))
			New ANOpCode(op_push);			// push the value to store on the stack
		Compile(child->next);				// compile index value
		if (theCode != op_lea)
			theCode |= OP_INDEX;				// set the indexing bit
		theAddr = child->val;
		varType = child->type;

	} else {
		theAddr = pn->val;
		varType = pn->type;
	}

	// Set the bits indicating the type of variable to be accessed, then
	// put out the opcode to access it.
	if (theCode == op_lea) {
		uint	accType;
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
		}

		if (indexed)
			accType |= OP_INDEX;
		an = New ANEffctAddr(theCode, theAddr, accType);

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
			}
		}

		if (theAddr < 256)
			theCode |= OP_BYTE;
		an = New ANVarAccess(theCode, theAddr);
	}

	// Put a pointer to the referenced symbol in the assembly node,
	// so we can print its name in the listing.
	switch (pn->type) {
		case PN_NUM:
			break;
		case PN_INDEX:
			an->sym = child->sym;
			break;
		default:
			an->sym = pn->sym;
			break;
	}
}

static void
MakeImmediate(int val)
{
	New ANOpSign(op_loadi, val);
}

static void
MakeString(PNode* pn)
{
	New ANOpOfs(pn->val);
}

static void
MakeCall(PNode* pn)
{
	// Compile a call to a procedure, the kernel, etc.

	// Push the number of arguments on the stack (we don't know the
	// number yet).
	ANOpUnsign* an = New ANOpUnsign(op_pushi, 0);

	// Compile the arguments.
	uint numArgs = MakeArgs(pn->child);

	// Put the number of arguments in its asmNode.
	an->value = numArgs;

	// Compile the call.
	Symbol* sym = pn->sym;
	if (pn->type == PN_CALL) {
		ANCall* call = New ANCall(sym);

		// If the destination procedure has not yet been defined, add
		// this node to the list of those waiting for its definition.
		if (sym->type == S_PROC && sym->val == UNDEFINED)
			call->addBackpatch(sym);
		else
			call->target = sym->loc;

		call->numArgs = 2 * numArgs;

	} else {
		Public* pub = sym->ext;
		ANOpExtern* extCall = New ANOpExtern(sym, pub->script, pub->entry);
		extCall->numArgs = 2 * numArgs;
	}
}

static void
MakeClassID(PNode* pn)
{
	// Compile a class ID.

	ANOpUnsign* an = New ANOpUnsign(op_class, pn->sym->obj->num);
	an->sym = pn->sym;
}

static void
MakeObjID(PNode* pn)
{
	// Compile an object ID.

	if (pn->sym->val == (int) OBJ_SELF)
		New ANOpCode(op_selfID);

	else {
		Symbol* sym = pn->sym;
		ANObjID* an = New ANObjID(sym);

		// If the object is not defined yet, add this node to the list
		// of those waiting for the definition.
		if (!sym->obj || sym->obj == curObj)
			an->addBackpatch(sym);
		else
			an->target = sym->obj->an;
	}
}

static void
MakeSend(PNode* pn)
{
	// Compile code for sending to an object through a handle stored in a
	// variable.

	// Get pointer to object node (an expression)
	PNode* on = pn->child;

	// Compile the messages to the object.
	int numArgs = 0;
	for (pn = on->next; pn; pn = pn->next)
		numArgs += MakeMessage(pn->child);

	// Add the appropriate send.
	ANSend* an;
	if (on->type == PN_OBJ && on->val == (int) OBJ_SELF)
		an = New ANSend(op_self);
	else if (on->type == PN_SUPER)
		an = New ANSuper(on->sym, on->val);
	else {
		Compile(on);				// compile the object/class id
		an = New ANSend(op_send);
	}

	an->numArgs = 2 * numArgs;
}

static int
MakeMessage(PNode* theMsg)
{
	// Compile code to push a message on the stack.

	// Compile the selector.
	Compile(theMsg);
	New ANOpCode(op_push);

	// Push the number of arguments on the stack (we don't know the
	// number yet).
	ANOpUnsign* an = New ANOpUnsign(op_pushi, (uint) -1);

	// Compile the arguments to the message and fix up the number
	// of arguments to the message.
	an->value = MakeArgs(theMsg->next);

	return an->value + 2;
}

static int
MakeArgs(PNode* theArg)
{
	// Compile code to push the arguments to a call on the stack.
	// Return the number of arguments.

	int numArgs = 0;

	for ( ; theArg; theArg = theArg->next) {
		if (theArg->type == PN_REST)
			New ANOpUnsign(op_rest | OP_BYTE, theArg->val);
		else {
			Compile(theArg);
			New ANOpCode(op_push);
			++numArgs;
		}
	}

	return numArgs;
}

static void
MakeUnary(PNode* pn)
{
	// Compile code for unary operators.

	// Do the argument to the operator.
	Compile(pn->child);

	// Put out the appropriate opcode.
	uword	theCode;
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
	New ANOpCode(theCode);
}

static void
MakeBinary(PNode* pn)
{
	// Compile code for a binary operator.

	// Compile the arguments, putting the first on the stack.
	Compile(pn->child);
	New ANOpCode(op_push);
	Compile(pn->child->next);

	// Put out the opcode.
	uword	theCode;
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
	New ANOpCode(theCode);
}

static void
MakeNary(PNode* pn)
{
	// Compile code for an nary operator (one with any number of arguments).

	// Compile the first argument and push its value on the stack.
	PNode* theArg = pn->child;
	Compile(theArg);
	New ANOpCode(op_push);

	for (theArg = theArg->next; theArg; ) {
		// Compile the next argument.
		Compile(theArg);

		// Put out the appropriate opcode.
		uword theCode;
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
		New ANOpCode(theCode);

		// Point to the next argument.  If there is one, push the current
		// result on the stack for combining with the next argument.  If
		// there is no next argument, we're done -- leave the result
		// in the register.
		theArg = theArg->next;
		if (theArg)
			New ANOpCode(op_push);
	}
}

static void
MakeAssign(PNode* pn)
{
	// If this is an arithmetic-op assignment, put the value of the
	// target variable on the stack for the operation.
	if (pn->val != A_EQ) {
		MakeAccess(pn->child, OP_LDST | OP_LOAD);
		New ANOpCode(op_push);
	}

	// Compile the value to be assigned.
	Compile(pn->child->next);

	// If this is an arithmetic-op assignment, do the arithmetic operation.
	uword	theCode;
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
		New ANOpCode(theCode);
	}

	MakeAccess(pn->child, OP_LDST | OP_STORE);
}

static void
MakeReturn(PNode* pn)
{
	// Compile code for a return.

	// If there was an argument to the return, compile it.
	if (pn->child)
		Compile(pn->child);

	// Put out the return opcode.
	New ANOpCode(op_ret);
}

void
MakeBranch(ubyte theCode, ANode* bn, Symbol* dest)
{
	// Compile code for a branch.  The type of branch is in 'theCode', the
	// destination is 'bn'.  If the the destination is not yet defined,
	// 'dest' will point to a the symbol of the destination.

	ANBranch* an = New ANBranch(theCode);

	// If the target of the branch has already been defined, point to
	// it.  Otherwise, add this node the the list of those waiting for
	// the target to be defined
	if (bn)
		an->target = bn;
	else if (dest)
		an->addBackpatch(dest);
	else
		Error("MakeBranch: bad call");
}

static void
MakeComp(PNode* pn)
{
	// Compile a comparison expression.  These expressions are nary expressions
	// with an early out -- the moment the truth value of the expression is
	// known, we stop evaluating the expression.

	// Get the comparison operator.
	uint op = pn->val;

	if (op == N_OR)
		// Handle special case of '||'.
		MakeOr(pn->child);
	else if (op == N_AND)
		// Handle special case of '&&'.
		MakeAnd(pn->child);
	else {
		// Point to the first argument and set up an empty need list (which
		// will be used to keep track of those nodes which need the address
		// of the end of the expression for the early out).
		PNode* node = pn->child;
		Symbol earlyOut;

		// Compile the first two operands and do the test.
		Compile(node);
		New ANOpCode(op_push);
		node = node->next;
		Compile(node);
		MakeCompOp(op);

		// If there are no more operands, we're done.  Otherwise we've got
		// to bail out of the test if it is already false or continue if it
		// is true so far.
		while (node = node->next) {
			// Early out if false.
			MakeBranch(op_bnt, 0, &earlyOut);

			// Push the previous accumulator value on the stack in
			// order to continue the comparison.
			New ANOpCode(op_pprev);

			// Compile the next argument and test it.
			Compile(node);
			MakeCompOp(op);
		}

		// Set the target for any branches to the end of the expression
		MakeLabel(&earlyOut);
	}
}

static void
MakeAnd(PNode* pn)
{
	// Compile code for the '&&' operator.

	Symbol	earlyOut;

	while (1) {
		// Compile an argument.
		Compile(pn);

		// If the argument just compiled was the last, the expression
		// evaluates to its value and we're done.
		pn = pn->next;
		if (!pn)
			break;

		// Make a branch for an early out if the expression is false.
		MakeBranch(op_bnt, 0, &earlyOut);
	}

	// Set the target for any early-out branches.
	MakeLabel(&earlyOut);
}

static void
MakeOr(PNode* pn)
{
	// Compile code for the '||' operator.

	Symbol	earlyOut;

	while (1) {
		// Compile code for an argument.
		Compile(pn);

		// If this is the last argument, the expression takes on its
		// value and we're done.
		pn = pn->next;
		if (!pn)
			break;

		// Make a branch for an early out if the expression is true.
		MakeBranch(op_bt, 0, &earlyOut);
	}

	// Make a target for the early-out branches.
	MakeLabel(&earlyOut);
}

static void
MakeCompOp(int op)
{
	// Compile the opcode corresponding to the comparison operator 'op'.

	uword	theCode;
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

	New ANOpCode(theCode);
}

static void
MakeIf(PNode* pn)
{
	// Compile code for an 'if' statement.

	// Compile the conditional expression
	PNode* node = pn->child;
	Compile(node);

	// Branch to the else code (if there is any) if the expression is false.
	Symbol elseLabel;
	MakeBranch(op_bnt, 0, &elseLabel);

	// Compile the code to be executed if expression was true.
	node = node->next;
	if (node)
		Compile(node);

	// If there is no 'else' code, we're done -- backpatch the branch.
	// Otherwise, jump around the else code, backpatch the jump to the
	// else code, compile the else code, and backpatch the jump around
	// the else code.
	if (node)
		node = node->next;
	if (!node)
		MakeLabel(&elseLabel);

	else {
		Symbol doneLabel;
		MakeBranch(op_jmp, 0, &doneLabel);
		MakeLabel(&elseLabel);
		Compile(node);
		MakeLabel(&doneLabel);
	}
}

static void
MakeCond(PNode* pn)
{
	// Compile code for a 'cond' expression.

	Symbol 	done;
	Symbol 	next;
	Bool		elseSeen = False;

	PNode* node = pn->child;
	while (node) {

		// The else clause needs no test before its execution.  Otherwise,
		// compile the code to test a condition and branch to the next
		// condition test if the condition is not true.
		if (node->type != PN_ELSE) {
			if (elseSeen)
				Error("Else must come at end of cond statement");

			// Compile the condition test.
			Compile(node);

			//	if we're at the end, just fall through
			if (!node->next)
				;

			//	if we're on the last test and it fails, exit
			else if (node->next->type == PN_ELIST && !node->next->next)
				MakeBranch(op_bnt, 0, &done);

			//	if we're on an interior test and it fails, go to next test
			else
				MakeBranch(op_bnt, 0, &next);

		} else if (elseSeen)
			Error("Multiple else clauses");

		else
			elseSeen = True;

		// Compile the statements to be executed if a condition was
		// satisfied.
		if (node->next && node->next->type == PN_ELIST) {
			node = node->next;
			Compile(node);
		}

		// If we're at the end of the cond clause, we're done.  Otherwise
		// make a jump to the end of the cond clause and compile a
		// destination for the jump which branched around the code
		// just compiled.
		node = node->next;
		if (node) {
			MakeBranch(op_jmp, 0, &done);
			MakeLabel(&next);
		}
	}

	// Make a destination for jumps to the end of the cond clause.
	MakeLabel(&done);
}

static void
MakeSwitch(PNode* pn)
{
	// Compile code for the 'switch' statement.

	Symbol	done;
	Symbol	next;
	Bool		elseSeen = False;

	// Compile the expression to be switched on and put the value on
	// the stack.
	PNode* node = pn->child;
	Compile(node);
	New ANOpCode(op_push);

	node = node->next;
	while (node) {
		// Compile the expression to compare the switch value to, then
		// test the values for equality.  Make a branch around the
		// code if the expressions are not equal.
		if (node->type != PN_ELSE) {
			if (elseSeen)
				Error("Else must come at end of switch statement");

			// Duplicate the switch value.
			New ANOpCode(op_dup);

			// Compile the test value.
			Compile(node);

			// Test for equality.
			New ANOpCode(op_eq);

			//	if we're at the end, just fall through
			if (!node->next)
				;

			//	if we're on the last test and it fails, exit
			else if (node->next->type == PN_ELIST && !node->next->next)
				MakeBranch(op_bnt, 0, &done);

			//	if we're on an interior test and it fails, go to next test
			else
				MakeBranch(op_bnt, 0, &next);

		} else if (elseSeen)
			Error("Multiple else clauses");

		else
			elseSeen = True;

		// Compile the statements to be executed if a switch matched.
		if (node->next->type == PN_ELIST) {
			node = node->next;
			Compile(node);
		}

		// If we're at the end of the switch expression, we're done.
		// Otherwise, make a jump to the end of the expression, then
		// make a target for the branch around the previous code.
		node = node->next;
		if (node) {
			MakeBranch(op_jmp, 0, &done);
			MakeLabel(&next);
		}
	}

	// Compile a target for jumps to the end of the switch expression.
	MakeLabel(&done);

	// Take the switch value off the stack.
	New ANOpCode(op_toss);
}

static void
MakeIncDec(PNode* pn)
{
	// Compile code for increment or decrement operators.

	uword	theCode;

	switch (pn->val) {
		case K_INC:
			theCode = OP_LDST | OP_INC;
			break;
		case K_DEC:
			theCode = OP_LDST | OP_DEC;
			break;
	}
	MakeAccess(pn->child, (ubyte) theCode);
}

static void
MakeProc(PNode* pn)
{
	// Compile code for an entire procedure.

	// Make a procedure node and point to the symbol for the procedure
	// (for listing purposes).
	ANCodeBlk* an = pn->type == PN_PROC ?
							(ANCodeBlk*) New ANProcCode(pn->sym) :
							(ANCodeBlk*) New ANMethCode(pn->sym);
	an->sym->type = (sym_t) (pn->type == PN_PROC ? S_PROC : S_SELECT);

	// If any nodes already compiled have this procedure as a target,
	// they will be on a list hanging off the procedure's symbol table
	// entry (in the 'ref' property) (compiled by the first reference to the
	// procedure).  Let all these nodes know where this one is.
	if (pn->sym->ref)
		pn->sym->ref->backpatch(an);
	pn->sym->loc = an;

	//	procedures and methods get special treatment:  the line number
	//	and file name are set here
	if (includeDebugInfo) {
		New ANLineNum(pn->lineNum);
		lastLineNum = pn->lineNum;
//		New ANFileName(curSourceFile->fileName);
	}

	// If there are to be any temporary variables, add a link node to
	// create them.
	if (pn->val)
		New ANOpUnsign(op_link, pn->val);

	// Compile code for the procedure followed by a return.
	if (pn->child)
		Compile(pn->child);

	if (includeDebugInfo) {
		assert(curSourceFile);
		New ANLineNum(curSourceFile->lineNum);
	}
	New ANOpCode(op_ret);

	an->finish();
}

void
MakeDispatch(int maxEntry)
{
	// Compile the dispatch table which goes at the start of this script.

	AList* oldList = curList;
	curList = &dispTbl->entries;

	// Now cycle through the publicly declared procedures/objects,
	// creating asmNodes for a table of their offsets.
	numDispTblEntries->value = maxEntry + 1;
	for (int i = 0; i <= maxEntry; ++i) {
		ANDispatch* an = New ANDispatch;
		if (an->sym = FindPublic(i))
			// Add this to the backpatch list of the symbol.
			an->addBackpatch(an->sym);
	}

	curList = oldList;
}

void
MakeObject(Object* theObj)
{
	AList* oldList = curList;
	curList = sc->heapList;

	// Create the object ID node.
	ANObject* obj = New ANObject(theObj->sym, theObj->num);
	theObj->an = obj;

	// Create the table of properties.
	ANTable* props = New ANTable("properties");
	ANOfsProp* pDict = 0;
	ANOfsProp* mDict = 0;

	Selector* sp = new Selector;
	for (sp = theObj->selectors; sp; sp = sp->next)
		if (IsProperty(sp)) {
			switch (sp->tag) {
				case T_PROP:
					New ANIntProp(sp->sym, sp->val);
					break;

				case T_TEXT:
					New ANTextProp(sp->sym, sp->val);
					break;

				case T_PROPDICT:
					pDict = New ANOfsProp(sp->sym);
					break;

				case T_METHDICT:
					mDict = New ANOfsProp(sp->sym);
					break;
			}
		}
	props->finish();

	// If any nodes already compiled have this object as a target, they
	// will be on a list hanging off the object's symbol table entry.
	// Let all nodes know where this one is.
	if (obj->sym->ref)
		obj->sym->ref->backpatch(props);
	obj->sym->loc = props;

	// The rest of the object goes into hunk, as it never changes.
	curList = sc->hunkList;
	New ANObject(theObj->sym, theObj->num, codeStart);

	// If this a class, add the property dictionary.
	ANObjTable* propDict = New ANObjTable("property dictionary");
	if (theObj->num != OBJECTNUM) {
		for (sp = theObj->selectors; sp; sp = sp->next)
			if (IsProperty(sp))
				New ANWord(sp->sym->val);
	}
	propDict->finish();
	if (pDict)
		pDict->target = propDict;

	ANObjTable* methDict = New ANObjTable("method dictionary");
	ANWord* numMeth = New ANWord((short) 0);
	for (sp = theObj->selectors; sp; sp = sp->next)
		if (sp->tag == T_LOCAL) {
			New ANWord(sp->sym->val);
			New ANMethod(sp->sym, (ANMethCode* ) sp->an);
			sp->sym->loc = 0;
			++numMeth->value;
		}
	methDict->finish();
	if (mDict)
		mDict->target = methDict;

	curList = oldList;
}

void
MakeText()
{
	// Add text strings to the assembled code.

	New ANWord(sc->heapList);	// terminate the object portion of the heap with a
									//	null word
	for (Text* tp = text.head; tp; tp = tp->next)
		New ANText(tp);
}

void
MakeLabel(Symbol* dest)
{
	if (dest->ref) {
		dest->ref->backpatch(New ANLabel);
		dest->ref = 0;
	}
}


