//	optimize.cpp	sc
// 	optimize generated assembly code

#include "sol.hpp"

#include	"jeff.hpp"

#include	"sc.hpp"

#include	"anode.hpp"
#include	"opcodes.hpp"
#include	"optimize.hpp"

enum {
	UNKNOWN = 0x4000,
	IMMEDIATE,
	PROP,
	OFS,
	SELF,
};

#define	indexed(op)		((op) & OP_INDEX)
#define	toStack(op)		((op) & OP_STACK)

uint
OptimizeProc(AList* al)
{
	uint		accType = UNKNOWN;
	int		accVal = 0;
	int		stackVal;
	uint		stackType = UNKNOWN;
	uint		nOptimizations = 0;

	for (ANOpSign* an = (ANOpSign*) al->first(); an; an =(ANOpSign*)al->next()) {
		Bool byteOp = an->op & OP_BYTE;
		uint op = an->op & ~OP_BYTE;

		switch (op) {
			case op_bnot:
			case op_neg:
			case op_not:
			case op_class:
			case op_lofsa:
				accType = UNKNOWN;
				break;

			case op_add:
			case op_sub:
			case op_mul:
			case op_div:
			case op_mod:
			case op_shr:
			case op_shl:
			case op_xor:
			case op_and:
			case op_or:
			case op_eq:
			case op_ne:
			case op_gt:
			case op_ge:
			case op_lt:
			case op_le:
			case op_ugt:
			case op_uge:
			case op_ult:
			case op_ule:
			case op_call:
			case op_callk:
			case op_callb:
			case op_calle:
			case op_send:
			case op_self:
			case op_super:
			case op_lea:
			case OP_LABEL:
			case op_lofss:
				accType = stackType = UNKNOWN;
				break;

			case op_link:
			case op_toss:
				stackType = UNKNOWN;
				break;

			case op_push:
				stackType = accType;
				stackVal = accVal;
				break;

			case op_push0:
				stackVal = 0;
				stackType = IMMEDIATE;
				break;

			case op_push1:
				stackVal = 1;
				stackType = IMMEDIATE;
				break;

			case op_push2:
				stackVal = 2;
				stackType = IMMEDIATE;
				break;

			case op_pushSelf:
				stackType = SELF;
				break;

			case op_pushi:
			{
				int val = an->value;
				if (val == 0) {
					al->replaceWith(an, New ANOpCode(op_push0));
					++nOptimizations;

				} else if (val == 1) {
					al->replaceWith(an, New ANOpCode(op_push1));
					++nOptimizations;

				} else if (val == 2) {
					al->replaceWith(an, New ANOpCode(op_push2));
					++nOptimizations;

				} else if (accType == IMMEDIATE && accVal == val) {
					// If accumulator already contains this value,
					// just push it.
					al->replaceWith(an, New ANOpCode(op_push));
					++nOptimizations;

				} else if (stackType == IMMEDIATE && stackVal == val) {
					// If stack already contains this value, dup it.
					al->replaceWith(an, New ANOpCode(op_dup));
					++nOptimizations;
				}

				stackType = IMMEDIATE;
				stackVal = val;
				break;
			}

			case op_ret:
				// Optimize out double returns.
				if (al->removeOp(op_ret))
					++nOptimizations;
				break;

			case op_loadi:
				if (al->removeOp(op_push)) {
					// Replace a load immediate followed by a push with
					// a push immediate.
					accType = UNKNOWN;
					stackType = IMMEDIATE;
					stackVal = an->value;
					op = byteOp ? op_pushi | OP_BYTE : op_pushi;
					al->replaceWith(an, New ANOpSign(op, stackVal));
					++nOptimizations;

				} else if (accType == IMMEDIATE && accVal == an->value) {
					// If acc already has this value, delete
					// this node.
					al->del(an);
					++nOptimizations;

				} else {
					accType = IMMEDIATE;
					accVal = an->value;
				}

				break;

			case op_bt:
			case op_bnt:
			case op_jmp:
			{
				// Eliminate branches to branches.
				ANode* label = ((ANBranch*) an)->target;
				while (label) {
					// 'label' points to the label to which we are branching.  Search
					// for the first op-code following this label.
					ANBranch* tmp = (ANBranch*) al->nextOp(label);

					// If the first op-code following the label is not a jump or
					// a branch of the same sense, no more optimization is possible.
					uint opType = tmp->op & ~OP_BYTE;
					if (opType != op_jmp && opType != (an->op & ~OP_BYTE))
						break;

					// We're pointing to another jump.  Make its label ou
					// destination and keep trying to optimize.
					if (tmp->target == label)
						label = 0;
					else {
						((ANBranch*) an)->target = label = tmp->target;
						++nOptimizations;
					}
				}
				break;
			}

			case op_ipToa:
			case op_dpToa:
				accType = accVal = UNKNOWN;
				break;

			case op_ipTos:
			case op_dpTos:
				stackType = stackVal = UNKNOWN;
				break;

			case op_pToa:
				if (al->removeOp(op_push)) {
					an->op = byteOp ? op_pTos | OP_BYTE : op_pTos;
					++nOptimizations;
					stackType = accType;
					stackVal = accVal;
					accType = UNKNOWN;
					if (indexed(op))
						stackType = UNKNOWN;

				} else if (accType == PROP &&
							  accVal == an->value && !indexed(op)) {
					al->del(an);
					++nOptimizations;

				} else if (indexed(op))
					accType = accVal = UNKNOWN;

				else {
					accType = PROP;
					accVal = an->value;
				}
				break;

			case op_pTos:
				if (indexed(op))
					stackType = stackVal = UNKNOWN;

				else if (accType == PROP && an->value == accVal) {
					// Replace a load to the stack with the acc's current
					// value by a push.
					al->replaceWith(an, New ANOpCode(op_push));
					++nOptimizations;

				} else if (stackType == PROP && an->value == stackVal) {
					// Replace a load to the stack of its current value
					// with a dup.
					al->replaceWith(an, New ANOpCode(op_dup));
					++nOptimizations;

				} else {
					// Update the stack's value.
					stackType = op & OP_VAR;
					stackVal = an->value;
				}
				break;

			case op_selfID:
				if (al->removeOp(op_push)) {
					an->op = op_pushSelf;
					stackType = SELF;
					++nOptimizations;

				} else {
					ANSend* rn = (ANSend*) al->findOp(op_send);
					if (rn) {
						an = (ANOpSign*) al->replaceWith(an, New ANSend(op_self));
						((ANSend *) an)->numArgs = rn->numArgs;
						al->del(rn);
						++nOptimizations;
						stackType = accType = UNKNOWN;

					} else
						accType = UNKNOWN;
				}
				break;

			default:
				if (!(op & OP_LDST))
					break;

				// We can only optimize loads -- others just set the value of the
				// accumulator.
				if ((op & OP_TYPE) != OP_LOAD) {
					if (indexed(op))
						accType = stackType = UNKNOWN;
					else {
						accType = op & OP_VAR;
						accVal = an->value;
					}
					break;
				}

				if (!toStack(op) && !indexed(op) &&
					 (op & OP_VAR) == accType && an->value == accVal) {
					// Then this just loads the acc with its present value.
					// Remove the node.
					al->del(an);
					++nOptimizations;
					break;
				}

				if (!toStack(op) && al->removeOp(op_push)) {
					// Replace a load followed by a push with a load directly
					// to the stack.
					stackType = accType;
					stackVal = accVal;
					accType = UNKNOWN;
					op = an->op |= OP_STACK;
					++nOptimizations;
				}

				if (!toStack(op)) {
					// Not a stack operation -- update accumulator's value.
					accType = op & OP_VAR;
					accVal = an->value;

				} else if ((op & OP_VAR) == accType && an->value == accVal) {
					// Replace a load to the stack with the acc's current
					// value by a push.
					al->replaceWith(an, New ANOpCode(op_push));
					stackType = accType;
					stackVal = accVal;
					++nOptimizations;

				} else if ((op & OP_VAR) == stackType && an->value == stackVal) {
					// Replace a load to the stack of its current value
					// with a dup.
					al->replaceWith(an, New ANOpCode(op_dup));
					++nOptimizations;

				} else if (indexed(op)) {
					stackType = stackVal = UNKNOWN;

				} else {
					// Update the stack's value.
					stackType = op & OP_VAR;
					stackVal = an->value;
				}
				break;
		}
	}

	return nOptimizations;
}

