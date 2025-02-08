//	parse.cpp	sc
// 	parse a script

#include "scic/parse.hpp"

#include <csetjmp>
#include <optional>
#include <string>

#include "scic/asm.hpp"
#include "scic/define.hpp"
#include "scic/error.hpp"
#include "scic/input.hpp"
#include "scic/parse_class.hpp"
#include "scic/parse_context.hpp"
#include "scic/parse_object.hpp"
#include "scic/pnode.hpp"
#include "scic/proc.hpp"
#include "scic/sc.hpp"
#include "scic/selector.hpp"
#include "scic/symbol.hpp"
#include "scic/symtbl.hpp"
#include "scic/symtypes.hpp"
#include "scic/token.hpp"
#include "scic/toktypes.hpp"

PNode::PNode(pn_t t)
    : sym(0), val(0), type(t), lineNum(gInputState.GetTopLevelLineNum()) {}

bool Parse() {
  InitAsm();

  gSyms.clearAsmPtrs();

  std::optional<TokenSlot> token;
  while ((token = NewToken())) {
    // We require an opening parenthesis at this level.
    // Keep reading until we get one.
    if (!OpenP(token->type())) {
      Error("Opening parenthesis expected: %s", token->name());
      while (token && !OpenP(token->type())) {
        token = NewToken();
      }
      if (!token) break;
    }

    // The original code ignores the return value of setjmp.
    (void)setjmp(gRecoverBuf);

    // The next token must be a keyword.  Dispatch to the appropriate
    // routines for the keyword.

    token = NewToken();
    if (token) {
      switch (Keyword(*token)) {
        case K_SCRIPTNUM: {
          auto script_num = GetNumber("Script #");
          if (script_num) {
            if (gScript != -1)
              Severe("Script # already defined to be %d.", gScript);
            else
              gScript = *script_num;
          }
          break;
        }

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
          Severe("Keyword required: %s", token->name());
          break;

        default:
          Severe("Not a top-level keyword: %s.", token->name());
      }
    }

    CloseBlock();
  }

  if (gTokenState.nestedCondCompile()) Error("#if without #endif");

  return !gNumErrors;
}

void Include() {
  auto token = GetToken();
  if (token.type() != S_IDENT && token.type() != S_STRING) {
    Severe("Need a filename: %s", token.name());
    return;
  }
  std::string filename(token.name());
  // We need to put this at the right syntactic level, so we GetToken to grab
  // the remaining parent before opening the file.
  token = GetToken();
  if (token.type() != CLOSE_P) {
    Severe("Expected closing parenthesis: %s", token.name());
    return;
  }

  // Push the file onto the stack.
  gInputState.OpenFileAsInput(filename, true);
}

bool OpenBlock() {
  auto token = GetToken();
  return OpenP(token.type());
}

bool CloseBlock() {
  auto token = GetToken();
  if (token.type() == CLOSE_P)
    return true;
  else {
    Severe("Expected closing parenthesis: %s", token.name());
    return false;
  }
}
