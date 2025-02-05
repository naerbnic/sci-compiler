//
//	Module:	chartype.h
//
//	Author:	Jeff Stephenson
//
// Header for character type table lookup

#ifndef CHARTYPE_H
#define CHARTYPE_H

bool IsSep(char c);
bool IsTok(char c);
bool IsDigit(char c);
bool IsHex(char c);
bool IsTerm(char c);
bool IsIncl(char c);

#endif
