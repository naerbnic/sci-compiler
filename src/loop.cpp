//	loop.cpp
// 	loop code generation

#include <cassert>

#include "anode.hpp"
#include "compile.hpp"
#include "opcodes.hpp"
#include "parse.hpp"
#include "symbol.hpp"

enum LoopType { LOOP_FOR, LOOP_WHILE, LOOP_REPEAT };

struct Loop {
  Loop(AList* curList, LoopType t, Symbol* c, Symbol* e);
  ~Loop();

  Loop* next;
  LoopType type;
  ANode* start;  // address of start of the loop
  Symbol* cont;  // symbol for continue address
  Symbol* end;   // symbol for the end of the loop
};

// 'loopList' is a stack of the currently active loops.  We can scan it
// to support such things as (break n) and (continue n).
Loop* loopList;

Loop::Loop(AList* curList, LoopType t, Symbol* c, Symbol* e)
    : next(loopList),
      type(t),
      start(curList->newNode<ANLabel>()),
      cont(c),
      end(e) {
  // Add this loop to the loop list.
  loopList = this;
}

Loop::~Loop() {
  // Remove this loop from the loop list.
  loopList = next;
}

void MakeWhile(AList* curList, PNode* theNode)
// while ::= 'while' expression statement*
{
  Symbol cont;
  Symbol end;
  Loop lp(curList, LOOP_WHILE, &cont, &end);

  cont.setLoc(lp.start);

  assert(theNode->children.size() == 2);

  // Compile the conditional expression controlling the loop,
  // and its corresponding branch.
  PNode* expr = theNode->child_at(0);
  PNode* body = theNode->child_at(1);
  Compile(curList, expr);
  MakeBranch(curList, op_bnt, 0, &end);

  // Compile the statements in the loop
  if (body) Compile(curList, body);

  // Make the branch back to the loop start.
  MakeBranch(curList, op_jmp, lp.start, 0);

  // Compile the label at the end of the loop
  MakeLabel(curList, &end);
}

void MakeRepeat(AList* curList, PNode* theNode) {
  // forever ::= 'forever' statement+

  Symbol cont;
  Symbol end;
  Loop lp(curList, LOOP_REPEAT, &cont, &end);

  cont.setLoc(lp.start);

  PNode* body = theNode->child_at(0);

  // Compile the loop's statements.
  if (body) Compile(curList, body);

  // Make the branch back to the start of the loop.
  MakeBranch(curList, op_jmp, lp.start, 0);

  // Make the target label for breaks.
  MakeLabel(curList, &end);
}

void MakeFor(AList* curList, PNode* theNode) {
  // for ::=	'for' '(' statement* ')'
  //			expression
  //			'(' statement* ')'
  //			statement*

  auto* init = theNode->child_at(0);
  auto* cond = theNode->child_at(1);
  auto* update = theNode->child_at(2);
  auto* body = theNode->child_at(3);

  // Make the initialization statements.
  if (init) Compile(curList, init);

  // Make the label at the start of the loop
  Symbol end;
  Symbol cont;
  Loop lp(curList, LOOP_FOR, &cont, &end);

  // Compile the conditional expression controlling the loop,
  // and its corresponding branch.
  if (cond) Compile(curList, cond);
  MakeBranch(curList, op_bnt, 0, &end);

  // Compile the statements in the loop
  if (body) Compile(curList, body);

  // Compile the re-initialization statements
  MakeLabel(curList, &cont);
  if (update) Compile(curList, update);

  // Make the branch back to the loop start.
  MakeBranch(curList, op_jmp, lp.start, 0);

  // Compile the label at the end of the loop
  MakeLabel(curList, &end);
}

void MakeBreak(AList* curList, PNode* theNode) {
  // break ::= 'break' [number]

  // Get the number of levels to break from.
  int level = theNode->val - 1;

  // Walk through the list of loops until we get to the proper
  // level to break to.  If the requested break level is greater
  // than the nesting level of the loops, go to the outermost loop.
  Loop* lp;
  for (lp = loopList; level > 0; --level)
    if (lp->next) lp = lp->next;

  MakeBranch(curList, op_jmp, 0, lp->end);
}

void MakeBreakIf(AList* curList, PNode* theNode) {
  // breakif ::= 'break' expression [number]

  // Get the number of levels to break from.
  int level = theNode->val - 1;

  // Compile the expression which determines whether or not we break.
  Compile(curList, theNode->first_child());

  // Walk through the list of loops until we get to the proper
  // level to break to.  If the requested break level is greater
  // than the nesting level of the loops, go to the outermost loop.
  Loop* lp;
  for (lp = loopList; level > 0; --level)
    if (lp->next) lp = lp->next;

  MakeBranch(curList, op_bt, 0, lp->end);
}

void MakeContinue(AList* curList, PNode* theNode) {
  // continue ::= 'continue' [number]

  // Get the number of levels to continue at.
  int level = theNode->val - 1;

  // Walk through the list of loops until we get to the proper
  // level to continue at.  If the requested continue level is greater
  // than the nesting level of the loops, go to the outermost loop.
  Loop* lp;
  for (lp = loopList; level > 0; --level)
    if (lp->next) lp = lp->next;

  if (lp->type == LOOP_FOR)
    MakeBranch(curList, op_jmp, 0, lp->cont);
  else
    MakeBranch(curList, op_jmp, lp->start, 0);
}

void MakeContIf(AList* curList, PNode* theNode) {
  // contif ::= 'contif' expression [number]

  // Get the number of levels to continue at.
  int level = theNode->val - 1;

  // Compile the expression which determines whether or not we continue.
  Compile(curList, theNode->first_child());

  // Walk through the list of loops until we get to the proper
  // level to continue at.  If the requested continue level is greater
  // than the nesting level of the loops, go to the outermost loop.
  Loop* lp;
  for (lp = loopList; level > 0; --level)
    if (lp->next) lp = lp->next;

  if (lp->type == LOOP_FOR)
    MakeBranch(curList, op_bt, 0, lp->cont);
  else
    MakeBranch(curList, op_bt, lp->start, 0);
}
