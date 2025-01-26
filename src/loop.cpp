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
  Loop(LoopType t, Symbol* c, Symbol* e);
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

Loop::Loop(LoopType t, Symbol* c, Symbol* e)
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

void MakeWhile(PNode* theNode)
// while ::= 'while' expression statement*
{
  Symbol cont;
  Symbol end;
  Loop lp(LOOP_WHILE, &cont, &end);

  cont.setLoc(lp.start);

  assert(theNode->children.size() == 2);

  // Compile the conditional expression controlling the loop,
  // and its corresponding branch.
  PNode* expr = theNode->child_at(0);
  PNode* body = theNode->child_at(1);
  Compile(expr);
  MakeBranch(op_bnt, 0, &end);

  // Compile the statements in the loop
  if (body) Compile(body);

  // Make the branch back to the loop start.
  MakeBranch(op_jmp, lp.start, 0);

  // Compile the label at the end of the loop
  MakeLabel(&end);
}

void MakeRepeat(PNode* theNode) {
  // forever ::= 'forever' statement+

  Symbol cont;
  Symbol end;
  Loop lp(LOOP_REPEAT, &cont, &end);

  cont.setLoc(lp.start);

  PNode* body = theNode->child_at(0);

  // Compile the loop's statements.
  if (body) Compile(body);

  // Make the branch back to the start of the loop.
  MakeBranch(op_jmp, lp.start, 0);

  // Make the target label for breaks.
  MakeLabel(&end);
}

void MakeFor(PNode* theNode) {
  // for ::=	'for' '(' statement* ')'
  //			expression
  //			'(' statement* ')'
  //			statement*

  auto* init = theNode->child_at(0);
  auto* cond = theNode->child_at(1);
  auto* update = theNode->child_at(2);
  auto* body = theNode->child_at(3);

  // Make the initialization statements.
  if (init) Compile(init);

  // Make the label at the start of the loop
  Symbol end;
  Symbol cont;
  Loop lp(LOOP_FOR, &cont, &end);

  // Compile the conditional expression controlling the loop,
  // and its corresponding branch.
  if (cond) Compile(cond);
  MakeBranch(op_bnt, 0, &end);

  // Compile the statements in the loop
  if (body) Compile(body);

  // Compile the re-initialization statements
  MakeLabel(&cont);
  if (update) Compile(update);

  // Make the branch back to the loop start.
  MakeBranch(op_jmp, lp.start, 0);

  // Compile the label at the end of the loop
  MakeLabel(&end);
}

void MakeBreak(PNode* theNode) {
  // break ::= 'break' [number]

  // Get the number of levels to break from.
  int level = theNode->val - 1;

  // Walk through the list of loops until we get to the proper
  // level to break to.  If the requested break level is greater
  // than the nesting level of the loops, go to the outermost loop.
  Loop* lp;
  for (lp = loopList; level > 0; --level)
    if (lp->next) lp = lp->next;

  MakeBranch(op_jmp, 0, lp->end);
}

void MakeBreakIf(PNode* theNode) {
  // breakif ::= 'break' expression [number]

  // Get the number of levels to break from.
  int level = theNode->val - 1;

  // Compile the expression which determines whether or not we break.
  Compile(theNode->first_child());

  // Walk through the list of loops until we get to the proper
  // level to break to.  If the requested break level is greater
  // than the nesting level of the loops, go to the outermost loop.
  Loop* lp;
  for (lp = loopList; level > 0; --level)
    if (lp->next) lp = lp->next;

  MakeBranch(op_bt, 0, lp->end);
}

void MakeContinue(PNode* theNode) {
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
    MakeBranch(op_jmp, 0, lp->cont);
  else
    MakeBranch(op_jmp, lp->start, 0);
}

void MakeContIf(PNode* theNode) {
  // contif ::= 'contif' expression [number]

  // Get the number of levels to continue at.
  int level = theNode->val - 1;

  // Compile the expression which determines whether or not we continue.
  Compile(theNode->first_child());

  // Walk through the list of loops until we get to the proper
  // level to continue at.  If the requested continue level is greater
  // than the nesting level of the loops, go to the outermost loop.
  Loop* lp;
  for (lp = loopList; level > 0; --level)
    if (lp->next) lp = lp->next;

  if (lp->type == LOOP_FOR)
    MakeBranch(op_bt, 0, lp->cont);
  else
    MakeBranch(op_bt, lp->start, 0);
}
