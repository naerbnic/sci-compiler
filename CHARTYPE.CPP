//	chartype.cpp
// 	character type lookup table.

#include "sol.hpp"

#include	"sc.hpp"

#include	"chartype.h"

ubyte	cType[256] = {
	C_SEP | C_TERM,									// ^@ NUL
	0,														// ^A SOH
	0,														// ^B STX
	0,														// ^C ETX
	0,														// ^D EOT
	0,														// ^E ENQ
	0,														// ^F ACK
	0,														// ^G BEL
	0,														// ^H BS
	C_SEP | C_TERM,									// ^I HT
	C_SEP | C_TERM,									// ^J LF
	0,														// ^K VT
	0,														// ^L FF
	C_SEP | C_TERM,									// ^M CR
	0,														// ^N SO
	0,														// ^O SI
	0,														// ^P DLE
	0,														// ^Q DC1
	0,														// ^R DC2
	0,														// ^S DC3
	0,														// ^T DC4
	0,														// ^U NAK
	0,														// ^V SYN
	0,														// ^W ETB
	0,														// ^X CAN
	0,														// ^Y EM
	0,														// ^Z SUB
	0,														// ^[ ESC
	0,														// ^\ FS
	0,														// ^] GS
	0,														// ^^ RS
	0,														// ^_ US
	C_SEP | C_TERM,									// space
	0,														// !
	0,														// "
	C_TOK,												// #
	C_DIGIT,												// $
	C_DIGIT,												// %
	0,														// &
	0,														// '
	C_TOK | C_TERM,									// (
	C_TOK | C_TERM,									// )
	0,														// *
	0,														// +
	C_TOK | C_TERM,									// ,
	0,														// -
	C_TOK,												// .
	0,														// /
	C_DIGIT | C_BIN | C_DEC | C_HEX,				// 0
	C_DIGIT | C_BIN | C_DEC | C_HEX,				// 1
	C_DIGIT | C_DEC | C_HEX,						// 2
	C_DIGIT | C_DEC | C_HEX,						// 3
	C_DIGIT | C_DEC | C_HEX,						// 4
	C_DIGIT | C_DEC | C_HEX,						// 5
	C_DIGIT | C_DEC | C_HEX,						// 6
	C_DIGIT | C_DEC | C_HEX,						// 7
	C_DIGIT | C_DEC | C_HEX,						// 8
	C_DIGIT | C_DEC | C_HEX,						// 9
	C_INCL,												// :
	C_SEP | C_TERM,									// ;
	0,														// <
	0,														// =
	0,														// >
	C_INCL,												// ?
	C_TOK,												// @
	C_HEX,												// A
	C_HEX,												// B
	C_HEX,												// C
	C_HEX,												// D
	C_HEX,												// E
	C_HEX,												// F
	0,														// G
	0,														// H
	0,														// I
	0,														// J
	0,														// K
	0,														// L
	0,														// M
	0,														// N
	0,														// O
	0,														// P
	0,														// Q
	0,														// R
	0,														// S
	0,														// T
	0,														// U
	0,														// V
	0,														// W
	0,														// X
	0,														// Y
	0,														// Z
	C_TOK | C_TERM,									// [
	0,														// backslash
	C_TOK | C_TERM,									// ]
	0,														// ^
	0,														// _
	0,														// `
	C_HEX,												// a
	C_HEX,												// b
	C_HEX,												// c
	C_HEX,												// d
	C_HEX,												// e
	C_HEX,												// f
};

