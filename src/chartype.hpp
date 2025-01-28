//
//	Module:	chartype.h
//
//	Author:	Jeff Stephenson
//
// Header for character type table lookup

#ifndef CHARTYPE_H
#define CHARTYPE_H

#include <cstdint>

#define C_SEP 0x0001    // seperator
#define C_TOK 0x0002    // single char token
#define C_DIGIT 0x0004  // first digit of a number
#define C_BIN 0x0008    // binary digit
#define C_DEC 0x0010    // decimal digit
#define C_HEX 0x0020    // hex digit
#define C_TERM 0x0040   // terminates an identifier
#define C_INCL 0x0080   // terminates ident, but included

#define IsSep(c) (cType[static_cast<uint8_t>(c)] & C_SEP)
#define IsTok(c) (cType[static_cast<uint8_t>(c)] & C_TOK)
#define IsDigit(c) (cType[static_cast<uint8_t>(c)] & C_DIGIT)
#define IsBin(c) (cType[static_cast<uint8_t>(c)] & C_BIN)
#define IsDec(c) (cType[static_cast<uint8_t>(c)] & C_DEC)
#define IsHex(c) (cType[static_cast<uint8_t>(c)] & C_HEX)
#define IsTerm(c) (cType[static_cast<uint8_t>(c)] & C_TERM)
#define IsIncl(c) (cType[static_cast<uint8_t>(c)] & C_INCL)

extern uint8_t cType[];

#endif
