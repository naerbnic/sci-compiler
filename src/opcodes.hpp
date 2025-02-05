//	opcodes.hpp

#ifndef OPCODES_HPP
#define OPCODES_HPP

/*******************************************************************************
 *
 *						OPCODES
 *
 *
 * The opcodes for SCI are partially bit-mapped.  The char is mapped as
 *
 * 	txxx xxxb
 *
 * where
 * 	t = 0	->	Arithmetic, stack, etc. operations
 * 	t = 1	->	Load/store operations
 *
 * 	b = 0	->	Following address/value is a int.
 * 	b = 1	->	Following address/value is a char.
 *
 *
 * The load/store operations are further bit-mapped:
 *
 * 	1boo idvv
 *
 * where
 * 	oo = 0	->	Load
 * 	oo = 1	->	Store
 * 	oo = 2	->	Increment, then load
 * 	oo = 3	->	Decrement, then load
 *
 * 	i = 0	->	Load/store from address as-is.
 * 	i = 1	->	Load/store indexed.  Index is in A.
 *
 *	d = 0	->	Load to accumulator
 *	d = 1	->	Load to stack
 *
 * 	vv = 0	->	Global
 * 	vv = 1	->	Local
 * 	vv = 2	->	Tmp
 * 	vv = 3	->	Parameter (load only -- specifies a different stack
 * 				frame than auto)
 *
 * Load operations leave the requested value in the A.  Store operations
 * store the A in memory unless they are indexed, in which case they store
 * the top value on the stack, popping the stack.
 ******************************************************************************/

const uint8_t OP_LDST = 0x80;  // load/store if set
const uint8_t OP_BYTE = 0x01;  // char operation if set, int otw

const uint8_t OP_TYPE = 0x60;   // mask for operation type
const uint8_t OP_LOAD = 0x00;   // load
const uint8_t OP_STORE = 0x20;  // store
const uint8_t OP_INC = 0x40;    // increment operation
const uint8_t OP_DEC = 0x60;    // decrement operation

const uint8_t OP_INDEX = 0x10;  // indexed op if set, non-indexed otw

const uint8_t OP_STACK = 0x08;  // load to stack if set

const uint8_t OP_VAR = 0x06;     // mask for var type
const uint8_t OP_GLOBAL = 0x00;  // global var
const uint8_t OP_LOCAL = 0x02;   // local var
const uint8_t OP_TMP = 0x04;     // temporary var (on the stack)
const uint8_t OP_PARM = 0x06;    // parameter (different stack frame than
                               // tmp)

// Opcodes other than load/store.
const uint8_t op_bnot = 0x00;
const uint8_t op_add = 0x02;
const uint8_t op_sub = 0x04;
const uint8_t op_mul = 0x06;
const uint8_t op_div = 0x08;
const uint8_t op_mod = 0x0A;
const uint8_t op_shr = 0x0C;
const uint8_t op_shl = 0x0E;
const uint8_t op_xor = 0x10;
const uint8_t op_and = 0x12;
const uint8_t op_or = 0x14;

const uint8_t op_neg = 0x16;
const uint8_t op_not = 0x18;

const uint8_t op_eq = 0x1A;
const uint8_t op_ne = 0x1C;
const uint8_t op_gt = 0x1E;
const uint8_t op_ge = 0x20;
const uint8_t op_lt = 0x22;
const uint8_t op_le = 0x24;

const uint8_t op_ugt = 0x26;
const uint8_t op_uge = 0x28;
const uint8_t op_ult = 0x2A;
const uint8_t op_ule = 0x2C;

const uint8_t op_bt = 0x2E;
const uint8_t op_bnt = 0x30;
const uint8_t op_jmp = 0x32;

const uint8_t op_loadi = 0x34;
const uint8_t op_push = 0x36;
const uint8_t op_pushi = 0x38;
const uint8_t op_toss = 0x3A;
const uint8_t op_dup = 0x3C;
const uint8_t op_link = 0x3E;

const uint8_t op_call = 0x40;
const uint8_t op_callk = 0x42;
const uint8_t op_callk_Word = 0x42;
const uint8_t op_callk_Char = 0x43;
const uint8_t op_callb = 0x44;
const uint8_t op_calle = 0x46;

const uint8_t op_ret = 0x48;

const uint8_t op_send = 0x4A;
// const uint8_t	op_sendk			= 0x4C;
// const uint8_t	op_sendb			= 0x4E;
const uint8_t op_class = 0x50;
// const uint8_t	op_objID			= 0x52;
const uint8_t op_self = 0x54;
const uint8_t op_super = 0x56;
const uint8_t op_rest = 0x58;
const uint8_t op_lea = 0x5A;
const uint8_t op_selfID = 0x5C;
// const uint8_t	op_superc		= 0x5E;
const uint8_t op_pprev = 0x60;

const uint8_t op_pToa = 0x62;
const uint8_t op_aTop = 0x64;
const uint8_t op_pTos = 0x66;
const uint8_t op_sTop = 0x68;
const uint8_t op_ipToa = 0x6A;
const uint8_t op_dpToa = 0x6C;
const uint8_t op_ipTos = 0x6E;
const uint8_t op_dpTos = 0x70;

const uint8_t op_lofsa = 0x72;
const uint8_t op_lofss = 0x74;

const uint8_t op_push0 = 0x76;
const uint8_t op_push1 = 0x78;
const uint8_t op_push2 = 0x7A;
const uint8_t op_pushSelf = 0x7C;

const uint8_t op_fileName = 0x7D;
const uint8_t op_lineNum = 0x7E;

const uint16_t OP_LABEL = 0x7000;

#endif
