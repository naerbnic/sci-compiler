//	optimize.cpp	sc
// 	optimize generated assembly code

#include "scic/codegen/optimize.hpp"

#include <cstdint>
#include <memory>
#include <ranges>
#include <stdexcept>

#include "scic/codegen/alist.hpp"
#include "scic/codegen/anode.hpp"
#include "scic/codegen/anode_impls.hpp"
#include "scic/codegen/list.hpp"
#include "scic/codegen/opcodes.hpp"
#include "util/types/casts.hpp"

namespace codegen {

ANOpCode const* FindNextOp(AList<ANOpCode> const* list, ANOpCode const* start) {
  if (!start) {
    throw new std::runtime_error("start cannot be nullptr");
  }

  for (auto& opcode :
       std::ranges::subrange(list->find(start).next(), list->end())) {
    if (opcode.op != OP_LABEL) {
      return &opcode;
    }
  }

  return nullptr;
}

#define isVarAccess(op) bool((op) & OP_LDST)
#define isStore(op) (((op) & OP_TYPE) == OP_STORE)
#define indexed(op) ((op) & OP_INDEX)
#define toStack(op) ((op) & OP_STACK)

// Returns true iff the given opcode reads from the accumulator.
bool OpReadsAccum(ANOpCode const* node) {
  // We don't care about the byte flag here.
  uint8_t op = node->op & ~OP_BYTE;

  // Operations are listed in opcode order, to make it easier to make sure
  // none are missed.
  switch (op) {
    // All math/logic ops use the accumulator.
    case op_bnot:
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
    case op_neg:
    case op_not:
      return true;

    // All comparison ops use the accumulator
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
      return true;

    // We can't tell about branch instructions, but depending on when/how they
    // branch, and what comes after. To be safe, assume that all branches use
    // the accumulator.
    case op_bt:
    case op_bnt:
    case op_jmp:
      return true;

    case op_loadi:
      return false;

    case op_push:
      return true;

    case op_pushi:
    case op_toss:
    case op_dup:
    case op_link:
      return false;

    case op_call:
      return true;

    // The non-local calls all use op parameters to choose which procedure to
    // call.
    case op_callk:
    case op_callb:
    case op_calle:
      return false;

    case op_ret:
    case op_send:
      return true;

    case op_class:
      return false;

    // self and super are like send, but take the object from the environment.
    // They do not read the accumulator.
    case op_self:
    case op_super:
      return false;

    case op_rest:
      return false;

    case op_lea: {
      // LEA is a bit trickier, as we have to look at the parameters to the
      // instruction to see if it's indexed, and thus reads the accumulator.
      auto* lea_node = down_cast<ANEffctAddr const>(node);
      return (lea_node->eaType & OP_INDEX) != 0;
    }

    case op_selfID:
    case op_pprev:
      return false;

    // Only the store accum to property instruction uses the accumulator.
    case op_pToa:
      return false;

    case op_aTop:
      return true;

    case op_pTos:
    case op_sTop:
    case op_ipToa:
    case op_dpToa:
    case op_ipTos:
    case op_dpTos:
      return false;

    case op_lofsa:
    case op_lofss:
    case op_push0:
    case op_push1:
    case op_push2:
    case op_pushSelf:
      return true;

    // Labels are pseudo-ops, and don't use the accumulator.
    case OP_LABEL:
      return false;

    default: {
      // This should be a variable access. Everything else should be an invalid
      // opcode.
      if (!isVarAccess(op)) {
        throw new std::runtime_error("Invalid opcode");
      }

      // There are two ways for this opcode to use the accumulator: Either it's
      // storing the accumulator to the variable, or it's indexing the variable
      // offset with the accumulator.
      return (isStore(op) && !toStack(op)) || indexed(op);
    }
  }
}

// Returns true iff the given opcode can modify the accumulator.
bool OpCanModifyAccum(ANOpCode const* node) {
  // We don't care about the byte flag here.
  uint8_t op = node->op & ~OP_BYTE;

  // Operations are listed in opcode order, to make it easier to make sure
  // none are missed.
  switch (op) {
    // All math/logic ops modify the accumulator.
    case op_bnot:
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
    case op_neg:
    case op_not:
      return true;

    // All comparison ops modify the accumulator
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
      return true;

    // Branch instructions don't modify the accumulator
    case op_bt:
    case op_bnt:
    case op_jmp:
      return false;

    case op_loadi:
      return true;

    case op_push:
      return false;

    case op_pushi:
    case op_toss:
    case op_dup:
    case op_link:
      return false;

    // All calls modify the accumulator
    case op_call:
    case op_callk:
    case op_callb:
    case op_calle:
      return true;

    case op_ret:
      return false;

    case op_send:
    case op_class:
    case op_self:
    case op_super:
      return true;

    case op_rest:
      return false;

    case op_lea:
    case op_selfID:
      return true;

    case op_pprev:
      return false;

    // Only the store accum to property instruction uses the accumulator.
    case op_pToa:
      return true;

    case op_aTop:
    case op_pTos:
    case op_sTop:
      return false;

    case op_ipToa:
    case op_dpToa:
      return true;

    case op_ipTos:
    case op_dpTos:
      return false;

    case op_lofsa:
      return true;
    case op_lofss:
    case op_push0:
    case op_push1:
    case op_push2:
    case op_pushSelf:
      return false;

    // Labels are pseudo-ops, and don't modify the accumulator.
    case OP_LABEL:
      return false;

    default: {
      // This should be a variable access. Everything else should be an invalid
      // opcode.
      if (!isVarAccess(op)) {
        throw new std::runtime_error("Invalid opcode");
      }

      // For this to modify the accumulator, it must be loading the variable
      // to the accumulator (which is all types aside from store).
      return !isStore(op) && !toStack(op);
    }
  }
}

// Returns true iff the given opcode can cause end execution of a sequence
// of opcodes.
bool OpChangesControlFlow(ANOpCode const* node) {
  // We don't care about the byte flag here.
  uint8_t op = node->op & ~OP_BYTE;

  // This is simpler than the other functions. Only the branch instructions
  // and return change control flow.
  switch (op) {
    case op_bt:
    case op_bnt:
    case op_jmp:
    case op_ret:
      return true;

    default:
      return false;
  }
}

// Returns if the instruction modifies the accumulator without reading it.
//
// This is useful to see if the accumulator value is important to later
// operations.
bool OpClobbersAccum(ANOpCode const* node) {
  return !OpReadsAccum(node) && OpCanModifyAccum(node);
}

// Returns true if the current value of the accumulator does not matter.
bool ExecutionClobbersAccum(TList<ANOpCode>::const_iterator it) {
  // This does a linear search through the opcode list, so could potentially
  // cause O(n^2) behavior. This should be fine for most cases, but we should
  // be aware for future optimizations.
  while (it) {
    if (OpReadsAccum(it.get())) {
      // The accumulator is used.
      return false;
    }
    if (OpCanModifyAccum(it.get())) {
      // The accumulator is overwritten.
      return true;
    }
    // The accumulator hasn't been changed by the instruction. Check the next
    // one.
    ++it;
  }

  throw std::runtime_error(
      "Got to end of opcode list without finding a branch or return.");
}

enum {
  UNKNOWN = 0x4000,
  IMMEDIATE,
  PROP,
  OFS,
  SELF,
};

uint32_t OptimizeProc(AOpList* al) {
  uint32_t accType = UNKNOWN;
  int accVal = 0;
  int stackVal;
  uint32_t stackType = UNKNOWN;
  uint32_t nOptimizations = 0;

  for (auto it = al->iter(); it; ++it) {
    bool byteOp = it->op & OP_BYTE;
    uint32_t op = it->op & ~OP_BYTE;

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

      case op_pushi: {
        int val = down_cast<ANOpSign>(it.get())->value;
        if (val == 0) {
          it.replaceWith(std::make_unique<ANOpCode>(op_push0));
          ++nOptimizations;

        } else if (val == 1) {
          it.replaceWith(std::make_unique<ANOpCode>(op_push1));
          ++nOptimizations;

        } else if (val == 2) {
          it.replaceWith(std::make_unique<ANOpCode>(op_push2));
          ++nOptimizations;

        } else if (accType == IMMEDIATE && accVal == val) {
          // If accumulator already contains this value,
          // just push it.
          it.replaceWith(std::make_unique<ANOpCode>(op_push));
          ++nOptimizations;

        } else if (stackType == IMMEDIATE && stackVal == val) {
          // If stack already contains this value, dup it.
          it.replaceWith(std::make_unique<ANOpCode>(op_dup));
          ++nOptimizations;
        }

        stackType = IMMEDIATE;
        stackVal = val;
        break;
      }

      case op_ret: {
        // Optimize out double returns.
        auto nextOp = it.next();
        if (nextOp && nextOp->op == op_ret) {
          nextOp.remove();
          ++nOptimizations;
        }
        break;
      }

      case op_loadi: {
        auto* an = down_cast<ANOpSign>(it.get());
        auto nextOp = it.next();
        if (nextOp->op == op_push) {
          // Replace a load immediate followed by a push with
          // a push immediate.
          nextOp.remove();
          accType = UNKNOWN;
          stackType = IMMEDIATE;
          stackVal = an->value;
          op = byteOp ? op_pushi | OP_BYTE : op_pushi;
          it.replaceWith(std::make_unique<ANOpSign>(op, stackVal));
          ++nOptimizations;
        } else if (accType == IMMEDIATE && accVal == an->value) {
          // If acc already has this value, delete
          // this node.
          it.remove();
          ++nOptimizations;

        } else {
          accType = IMMEDIATE;
          accVal = an->value;
        }

        break;
      }

      case op_bt:
      case op_bnt:
      case op_jmp: {
        // Eliminate branches to branches.
        ANLabel const* label = ((ANBranch*)it.get())->target;
        while (label) {
          // 'label' points to the label to which we are branching.  Search
          // for the first op-code following this label.
          ANBranch* tmp = (ANBranch*)FindNextOp(al, label);

          // If the first op-code following the label is not a jump or
          // a branch of the same sense, no more optimization is possible.
          uint32_t opType = tmp->op & ~OP_BYTE;
          if (opType != op_jmp && opType != (it->op & ~OP_BYTE)) break;

          // We're pointing to another jump.  Make its label ou
          // destination and keep trying to optimize.
          if (tmp->target == label)
            label = 0;
          else {
            ((ANBranch*)it.get())->target = label = tmp->target;
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

      case op_pToa: {
        auto* an = down_cast<ANOpSign>(it.get());
        auto nextOp = it.next();
        if (nextOp->op == op_push) {
          nextOp.remove();
          op = an->op = byteOp ? op_pTos | OP_BYTE : op_pTos;
          ++nOptimizations;
          stackType = accType;
          stackVal = accVal;
          accType = UNKNOWN;
          if (indexed(op)) stackType = UNKNOWN;

        } else if (accType == PROP && accVal == an->value && !indexed(op)) {
          it.remove();
          ++nOptimizations;

        } else if (indexed(op))
          accType = accVal = UNKNOWN;

        else {
          accType = PROP;
          accVal = an->value;
        }
        break;
      }

      case op_pTos: {
        auto* an = down_cast<ANOpSign>(it.get());
        if (indexed(op)) {
          stackType = stackVal = UNKNOWN;

        } else if (accType == PROP && an->value == accVal) {
          // Replace a load to the stack with the acc's current
          // value by a push.
          it.replaceWith(std::make_unique<ANOpCode>(op_push));
          ++nOptimizations;

        } else if (stackType == PROP && an->value == stackVal) {
          // Replace a load to the stack of its current value
          // with a dup.
          it.replaceWith(std::make_unique<ANOpCode>(op_dup));
          ++nOptimizations;

        } else {
          // Update the stack's value.
          stackType = op & OP_VAR;
          stackVal = an->value;
        }
        break;
      }

      case op_selfID: {
        auto nextOp = it.next();
        if (nextOp->op == op_push) {
          nextOp.remove();
          it->op = op_pushSelf;
          stackType = SELF;
          ++nOptimizations;

        } else {
          auto nextOpIt = it.next();
          if (nextOpIt->op == op_send) {
            ANSend* nextOp = static_cast<ANSend*>(nextOpIt.get());
            it.replaceWith(
                std::make_unique<ANSend>(nextOp->sci_target, op_self));
            ANSend* an = static_cast<ANSend*>(it.get());
            an->numArgs = nextOp->numArgs;
            nextOpIt.remove();
            ++nOptimizations;
            stackType = accType = UNKNOWN;

          } else
            accType = UNKNOWN;
        }
        break;
      }

      default: {
        if (!(op & OP_LDST)) break;

        auto* an = down_cast<ANVarAccess>(it.get());

        // We can only optimize loads -- others just set the value of the
        // accumulator.
        if ((op & OP_TYPE) == OP_STORE) {
          // The main kind of optimization we can do here is to alter this
          // instruction depending on the previous one.
          auto prevOp = it.prev();

          if (prevOp->op == op_push && toStack(op) && !indexed(op)) {
            // We pushing a value from the accumulator, then storing it
            // from the stack, without using the accumulator. Remove the
            // push, and change this to a store from the stack.
            prevOp.remove();
            op = an->op = an->op & ~OP_STACK;
            // We no longer know the state of the stack from before.
            stackType = stackVal = UNKNOWN;
            ++nOptimizations;
          }

          if (toStack(op)) {
            // We're popping the top of the stack in the store, so we
            // don't have any information anymore.
            stackType = stackVal = UNKNOWN;
          }
          accType = stackType = UNKNOWN;
          break;
        } else if ((op & OP_TYPE) != OP_LOAD) {
          // This is inc/dec, which have implicit loads as well.
          if (indexed(op))
            accType = stackType = UNKNOWN;
          else {
            accType = op & OP_VAR;
            accVal = an->addr;
          }
          break;
        }

        if (!toStack(op) && !indexed(op) && (op & OP_VAR) == accType &&
            an->addr == accVal) {
          // Then this just loads the acc with its present value.
          // Remove the node.
          it.remove();
          ++nOptimizations;
          break;
        }

        {
          // Try to optimize out a load followed by a push to a simple stack
          // load.
          //
          // We have to be careful, because if later code depends on the
          // accumulator, this could be unsound. (Ask me how I know...)
          auto nextOp = it.next();

          if (!toStack(op) && nextOp->op == op_push &&
              ExecutionClobbersAccum(nextOp.next())) {
            nextOp.remove();
            // Replace a load followed by a push with a load directly
            // to the stack.
            stackType = accType;
            stackVal = accVal;
            accType = UNKNOWN;
            op = an->op |= OP_STACK;
            ++nOptimizations;
          }
        }

        if (!toStack(op)) {
          // Not a stack operation -- update accumulator's value.
          accType = op & OP_VAR;
          accVal = an->addr;
        } else if ((op & OP_VAR) == accType && an->addr == accVal) {
          // Replace a load to the stack with the acc's current
          // value by a push.
          it.replaceWith(std::make_unique<ANOpCode>(op_push));
          stackType = accType;
          stackVal = accVal;
          ++nOptimizations;

        } else if ((op & OP_VAR) == stackType && an->addr == stackVal) {
          // Replace a load to the stack of its current value
          // with a dup.
          it.replaceWith(std::make_unique<ANOpCode>(op_dup));
          ++nOptimizations;

        } else if (indexed(op)) {
          stackType = stackVal = UNKNOWN;

        } else {
          // Update the stack's value.
          stackType = op & OP_VAR;
          stackVal = an->addr;
        }
        break;
      }
    }
  }

  return nOptimizations;
}

}  // namespace codegen
