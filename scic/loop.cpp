//	loop.cpp
// 	loop code generation

#include <cassert>

#include "scic/alist.hpp"
#include "scic/anode_impls.hpp"
#include "scic/compile.hpp"
#include "scic/opcodes.hpp"
#include "scic/pnode.hpp"
#include "util/types/forward_ref.hpp"

enum LoopType { LOOP_FOR, LOOP_WHILE, LOOP_REPEAT };

struct Loop {
  Loop(AOpList* curList, LoopType t, ForwardRef<ANLabel*>* c,
       ForwardRef<ANLabel*>* e);
  ~Loop();

  Loop* next;
  LoopType type;
  ANLabel* start;                    // address of start of the loop
  ForwardRef<ANLabel*>* cont;  // symbol for continue address
  ForwardRef<ANLabel*>* end;   // symbol for the end of the loop
};

// 'loopList' is a stack of the currently active loops.  We can scan it
// to support such things as (break n) and (continue n).
Loop* loopList;

Loop::Loop(AOpList* curList, LoopType t, ForwardRef<ANLabel*>* c,
           ForwardRef<ANLabel*>* e)
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

void MakeWhile(AOpList* curList, PNode* theNode)
// while ::= 'while' expression statement*
{
  ForwardRef<ANLabel*> cont;
  ForwardRef<ANLabel*> end;
  Loop lp(curList, LOOP_WHILE, &cont, &end);

  cont.Resolve(lp.start);

  assert(theNode->children.size() == 2);

  // Compile the conditional expression controlling the loop,
  // and its corresponding branch.
  PNode* expr = theNode->child_at(0);
  PNode* body = theNode->child_at(1);
  CompileExpr(curList, expr);
  MakeBranch(curList, op_bnt, &end);

  // Compile the statements in the loop
  if (body) CompileExpr(curList, body);

  // Make the branch back to the loop start.
  MakeBranch(curList, op_jmp, lp.start);

  // Compile the label at the end of the loop
  MakeLabel(curList, &end);
}

void MakeRepeat(AOpList* curList, PNode* theNode) {
  // forever ::= 'forever' statement+

  ForwardRef<ANLabel*> cont;
  ForwardRef<ANLabel*> end;
  Loop lp(curList, LOOP_REPEAT, &cont, &end);

  cont.Resolve(lp.start);

  PNode* body = theNode->child_at(0);

  // Compile the loop's statements.
  if (body) CompileExpr(curList, body);

  // Make the branch back to the start of the loop.
  MakeBranch(curList, op_jmp, lp.start);

  // Make the target label for breaks.
  MakeLabel(curList, &end);
}

void MakeFor(AOpList* curList, PNode* theNode) {
  // for ::=	'for' '(' statement* ')'
  //			expression
  //			'(' statement* ')'
  //			statement*

  auto* init = theNode->child_at(0);
  auto* cond = theNode->child_at(1);
  auto* update = theNode->child_at(2);
  auto* body = theNode->child_at(3);

  // Make the initialization statements.
  if (init) CompileExpr(curList, init);

  // Make the label at the start of the loop
  ForwardRef<ANLabel*> end;
  ForwardRef<ANLabel*> cont;
  Loop lp(curList, LOOP_FOR, &cont, &end);

  // Compile the conditional expression controlling the loop,
  // and its corresponding branch.
  if (cond) CompileExpr(curList, cond);
  MakeBranch(curList, op_bnt, &end);

  // Compile the statements in the loop
  if (body) CompileExpr(curList, body);

  // Compile the re-initialization statements
  MakeLabel(curList, &cont);
  if (update) CompileExpr(curList, update);

  // Make the branch back to the loop start.
  MakeBranch(curList, op_jmp, lp.start);

  // Compile the label at the end of the loop
  MakeLabel(curList, &end);
}

void MakeBreak(AOpList* curList, PNode* theNode) {
  // break ::= 'break' [number]

  // Get the number of levels to break from.
  int level = theNode->val - 1;

  // Walk through the list of loops until we get to the proper
  // level to break to.  If the requested break level is greater
  // than the nesting level of the loops, go to the outermost loop.
  Loop* lp;
  for (lp = loopList; level > 0; --level)
    if (lp->next) lp = lp->next;

  MakeBranch(curList, op_jmp, lp->end);
}

void MakeBreakIf(AOpList* curList, PNode* theNode) {
  // breakif ::= 'break' expression [number]

  // Get the number of levels to break from.
  int level = theNode->val - 1;

  // Compile the expression which determines whether or not we break.
  CompileExpr(curList, theNode->first_child());

  // Walk through the list of loops until we get to the proper
  // level to break to.  If the requested break level is greater
  // than the nesting level of the loops, go to the outermost loop.
  Loop* lp;
  for (lp = loopList; level > 0; --level)
    if (lp->next) lp = lp->next;

  MakeBranch(curList, op_bt, lp->end);
}

void MakeContinue(AOpList* curList, PNode* theNode) {
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
    MakeBranch(curList, op_jmp, lp->cont);
  else
    MakeBranch(curList, op_jmp, lp->start);
}

void MakeContIf(AOpList* curList, PNode* theNode) {
  // contif ::= 'contif' expression [number]

  // Get the number of levels to continue at.
  int level = theNode->val - 1;

  // Compile the expression which determines whether or not we continue.
  CompileExpr(curList, theNode->first_child());

  // Walk through the list of loops until we get to the proper
  // level to continue at.  If the requested continue level is greater
  // than the nesting level of the loops, go to the outermost loop.
  Loop* lp;
  for (lp = loopList; level > 0; --level)
    if (lp->next) lp = lp->next;

  if (lp->type == LOOP_FOR)
    MakeBranch(curList, op_bt, lp->cont);
  else
    MakeBranch(curList, op_bt, lp->start);
}
