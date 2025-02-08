//	parse.cpp	sc
// 	parse a script

#include "scic/parse.hpp"

#include <csetjmp>
#include <string>

#include "scic/asm.hpp"
#include "scic/class.hpp"
#include "scic/define.hpp"
#include "scic/error.hpp"
#include "scic/input.hpp"
#include "scic/object.hpp"
#include "scic/pnode.hpp"
#include "scic/proc.hpp"
#include "scic/sc.hpp"
#include "scic/selector.hpp"
#include "scic/symbol.hpp"
#include "scic/symtbl.hpp"
#include "scic/symtypes.hpp"
#include "scic/token.hpp"
#include "scic/toktypes.hpp"

jmp_buf gRecoverBuf;

PNode::PNode(pn_t t)
    : sym(0), val(0), type(t), lineNum(gInputState.GetTopLevelLineNum()) {}

bool Parse() {
  InitAsm();

  gSyms.clearAsmPtrs();

  while (NewToken()) {
    // We require an opening parenthesis at this level.
    // Keep reading until we get one.
    if (!OpenP(symType())) {
      Error("Opening parenthesis expected: %s", gTokenState.symStr());
      while (!OpenP(symType()) && symType() != S_END) NewToken();
      if (symType() == S_END) break;
    }

    // The original code ignores the return value of setjmp.
    (void)setjmp(gRecoverBuf);

    // The next token must be a keyword.  Dispatch to the appropriate
    // routines for the keyword.

    NewToken();
    switch (Keyword()) {
      case K_SCRIPTNUM:
        if (GetNumber("Script #")) {
          if (gScript != -1)
            Severe("Script # already defined to be %d.", gScript);
          else
            gScript = symVal();
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
        Severe("Keyword required: %s", gTokenState.symStr());
        break;

      default:
        Severe("Not a top-level keyword: %s.", gTokenState.symStr());
    }

    CloseBlock();
  }

  if (gTokenState.nestedCondCompile()) Error("#if without #endif");

  return !gNumErrors;
}

void Include() {
  GetToken();
  if (symType() != S_IDENT && symType() != S_STRING) {
    Severe("Need a filename: %s", gTokenState.symStr());
    return;
  }
  std::string filename = gTokenState.symStr();
  // We need to put this at the right syntactic level, so we GetToken to grab
  // the remaining parent before opening the file.
  GetToken();
  if (symType() != CLOSE_P) {
    Severe("Expected closing parenthesis: %s", gTokenState.symStr());
    return;
  }

  // Push the file onto the stack.
  gInputState.OpenFileAsInput(filename, true);
}

bool OpenBlock() {
  GetToken();
  return OpenP(symType());
}

bool CloseBlock() {
  GetToken();
  if (symType() == CLOSE_P)
    return true;
  else {
    Severe("Expected closing parenthesis: %s", gTokenState.symStr());
    return false;
  }
}
