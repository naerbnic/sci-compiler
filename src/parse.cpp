//	parse.cpp	sc
// 	parse a script

#include "parse.hpp"

#include <stdio.h>
#include <stdlib.h>

#include "asm.hpp"
#include "define.hpp"
#include "error.hpp"
#include "input.hpp"
#include "object.hpp"
#include "sc.hpp"
#include "symtbl.hpp"
#include "token.hpp"

jmp_buf gRecoverBuf;

PNode::PNode(pn_t t)
    : sym(0),
      val(0),
      type(t),
      lineNum(gInputState.curSourceFile ? gInputState.curSourceFile->lineNum
                                        : 0) {}

bool Parse() {
  InitAsm();

  gSyms.clearAsmPtrs();

  while (NewToken()) {
    // We require an opening parenthesis at this level.
    // Keep reading until we get one.
    if (!OpenP(symType)) {
      Error("Opening parenthesis expected: %s", gSymStr);
      while (!OpenP(symType) && symType != S_END) NewToken();
      if (symType == S_END) break;
    }

    setjmp(gRecoverBuf);

    // The next token must be a keyword.  Dispatch to the appropriate
    // routines for the keyword.

    NewToken();
    switch (Keyword()) {
      case K_SCRIPTNUM:
        if (GetNumber("Script #")) {
          if (gScript != -1)
            Severe("Script # already defined to be %d.", gScript);
          else
            gScript = symVal;
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

      case K_GLOBALDECL:
        GlobalDecl();
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
        Severe("Keyword required: %s", gSymStr);
        break;

      default:
        Severe("Not a top-level keyword: %s.", gSymStr);
    }

    CloseBlock();
  }

  if (gNestedCondCompile) Error("#if without #endif");

  return !gNumErrors;
}

void Include() {
  GetToken();
  if (symType != S_IDENT && symType != S_STRING)
    Severe("Need a filename: %s", gSymStr);
  else {
    gInputState.OpenFileAsInput(gSymStr, true);
  }
}

bool OpenBlock() {
  GetToken();
  return OpenP(symType);
}

bool CloseBlock() {
  GetToken();
  if (symType == CLOSE_P)
    return true;
  else {
    Severe("Expected closing parenthesis: %s", gSymStr);
    return false;
  }
}
