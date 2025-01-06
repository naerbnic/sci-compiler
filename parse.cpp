//	parse.cpp	sc
// 	parse a script

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "sol.hpp"

#include	"sc.hpp"

#include	"asm.hpp"
#include	"define.hpp"
#include	"error.hpp"
#include	"input.hpp"
#include	"object.hpp"
#include	"parse.hpp"
#include	"symtbl.hpp"
#include	"token.hpp"

jmp_buf	recoverBuf;

PNode::PNode(pn_t t) :
	next(0), child(0), sym(0), val(0), type(t),
	lineNum(curSourceFile ? curSourceFile->lineNum : 0)
{
}

Bool
Parse()
{
	InitAsm();

	syms.clearAsmPtrs();

	while (NewToken()) {
		// We require an opening parenthesis at this level.
		// Keep reading until we get one.
		if (!OpenP(symType)) {
			Error("Opening parenthesis expected: %s", symStr);
			while (!OpenP(symType) && symType != S_END)
				NewToken();
			if (symType == S_END)
				break;
		}

		setjmp(recoverBuf);

		//	set this position in the input stream as the beginning of the parse
		SetParseStart();

		// The next token must be a keyword.  Dispatch to the appropriate
		// routines for the keyword.

		NewToken();
		switch (Keyword()) {
			case K_SCRIPTNUM:
				if (GetNumber("Script #")) {
					if (script != -1)
						Severe("Script # already defined to be %d.", script);
					else
						script = symVal;
				}
				break;

			case K_INCLUDE:
				Include();
				continue;

			case K_PUBLIC:
				DoPublic();
				break;

			case K_EXTERN:
				Extern();
				break;

			case K_GLOBAL:
				Global();
				break;

			case K_LOCAL:
				Local();
				break;

			case K_DEFINE:
				Define();
				break;

			case K_ENUM:
				Enum();
				break;

			case K_PROC:
				Procedure();
				break;

			case K_CLASS:
				DoClass();
				break;

			case K_INSTANCE:
				Instance();
				break;

			case K_CLASSDEF:
				DefineClass();
				break;

			case K_SELECT:
				InitSelectors();
				break;

			case K_UNDEFINED:
				Severe("Keyword required: %s", symStr);
				break;

			default:
				Severe("Not a top-level keyword: %s.", symStr);
		}

		CloseBlock();
	}

	if (nestedCondCompile)
		Error("#if without #endif");

	return !errors;
}

void
Include()
{
	char	fileName[_MAX_PATH + 1];

	GetToken();
	if (symType != S_IDENT && symType != S_STRING)
		Severe("Need a filename: %s", symStr);
	else {
		strcpy(fileName, symStr);
		OpenFileAsInput(fileName, True);
	}
}

Bool
OpenBlock()
{
	GetToken();
	return OpenP(symType);
}

Bool
CloseBlock()
{
	GetToken();
	if (symType == CLOSE_P)
		return True;
	else {
		Severe("Expected closing parenthesis: %s", symStr);
		return False;
	}
}
