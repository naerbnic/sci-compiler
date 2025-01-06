//
//	Module:	chartype.h
//
//	Author:	Jeff Stephenson
//
// Header for character type table lookup


#if	!defined(CHARTYPE_H)
#define	CHARTYPE_H

#if	!defined(SC_HPP)
#include	"sc.hpp"
#endif

#define	C_SEP		0x0001		// seperator
#define	C_TOK		0x0002		// single char token
#define	C_DIGIT	0x0004		// first digit of a number
#define	C_BIN		0x0008		// binary digit
#define	C_DEC		0x0010		// decimal digit
#define	C_HEX		0x0020		// hex digit
#define	C_TERM	0x0040		// terminates an identifier
#define	C_INCL	0x0080		// terminates ident, but included

#define	IsSep(c)		(cType[c] & C_SEP)
#define	IsTok(c)		(cType[c] & C_TOK)
#define	IsDigit(c)	(cType[c] & C_DIGIT)
#define	IsBin(c)		(cType[c] & C_BIN)
#define	IsDec(c)		(cType[c] & C_DEC)
#define	IsHex(c)		(cType[c] & C_HEX)
#define	IsTerm(c)	(cType[c] & C_TERM)
#define	IsIncl(c)	(cType[c] & C_INCL)

#define	_tolower(c)	((c) | ('a' - 'A'))
#define	islower(c)	('a' <= (c) && (c) <= 'z')
#define	isupper(c)	('A' <= (c) && (c) <= 'Z')
#ifdef isalpha
#undef isalpha
#endif
#define	isalpha(c)	(islower(c) || isupper(c))
#ifdef toupper
#undef toupper
#endif
#define	toupper(c)	(islower(c)? (c) - ('a' - 'A') : (c))
#ifdef isdigit
#undef isdigit
#endif
#define	isdigit(c)	('0' <= (c) && (c) <=   '9')

extern	ubyte	cType[];

#endif
