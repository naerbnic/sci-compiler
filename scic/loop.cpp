//	loop.cpp
// 	loop code generation

#include <cassert>

#include "scic/codegen/code_generator.hpp"
#include "scic/compile.hpp"
#include "scic/pnode.hpp"

using ::codegen::FunctionBuilder;
using ::codegen::LabelRef;

enum LoopType { LOOP_FOR, LOOP_WHILE, LOOP_REPEAT };

struct Loop {
  Loop(LoopType t, LabelRef* cont, LabelRef* end);
  ~Loop();

  Loop* next;
  LoopType type;
  LabelRef* cont;  // symbol for continue address
  LabelRef* end;   // symbol for the end of the loop
};

// 'loopList' is a stack of the currently active loops.  We can scan it
// to support such things as (break n) and (continue n).
Loop* loopList;

Loop::Loop(LoopType t, LabelRef* cont, LabelRef* end)
    : next(loopList), type(t), cont(cont), end(end) {
  // Add this loop to the loop list.
  loopList = this;
}

Loop::~Loop() {
  // Remove this loop from the loop list.
  loopList = next;
}

void MakeWhile(FunctionBuilder* builder, PNode* theNode)
// while ::= 'while' expression statement*
{
  auto cont = builder->CreateLabelRef();
  auto end = builder->CreateLabelRef();
  Loop lp(LOOP_WHILE, &cont, &end);

  builder->AddLabel(&cont);

  assert(theNode->children.size() == 2);

  // Compile the conditional expression controlling the loop,
  // and its corresponding branch.
  PNode* expr = theNode->child_at(0);
  PNode* body = theNode->child_at(1);
  CompileExpr(builder, expr);
  builder->AddBranchOp(FunctionBuilder::BNT, &end);

  // Compile the statements in the loop
  if (body) CompileExpr(builder, body);

  // Make the branch back to the loop start.
  builder->AddBranchOp(FunctionBuilder::JMP, &cont);

  // Compile the label at the end of the loop
  builder->AddLabel(&end);
}

void MakeRepeat(FunctionBuilder* builder, PNode* theNode) {
  // forever ::= 'forever' statement+

  auto cont = builder->CreateLabelRef();
  auto end = builder->CreateLabelRef();
  Loop lp(LOOP_REPEAT, &cont, &end);

  builder->AddLabel(&cont);

  PNode* body = theNode->child_at(0);

  // Compile the loop's statements.
  if (body) CompileExpr(builder, body);

  // Make the branch back to the start of the loop.
  builder->AddBranchOp(FunctionBuilder::JMP, &cont);

  // Make the target label for breaks.
  builder->AddLabel(&end);
}

void MakeFor(FunctionBuilder* builder, PNode* theNode) {
  // for ::=	'for' '(' statement* ')'
  //			expression
  //			'(' statement* ')'
  //			statement*

  auto* init = theNode->child_at(0);
  auto* cond = theNode->child_at(1);
  auto* update = theNode->child_at(2);
  auto* body = theNode->child_at(3);

  // Make the initialization statements.
  if (init) CompileExpr(builder, init);

  // Make the label at the start of the loop
  auto end = builder->CreateLabelRef();
  auto cont = builder->CreateLabelRef();
  auto start = builder->CreateLabelRef();
  builder->AddLabel(&start);

  Loop lp(LOOP_FOR, &cont, &end);

  // Compile the conditional expression controlling the loop,
  // and its corresponding branch.
  if (cond) CompileExpr(builder, cond);
  builder->AddBranchOp(FunctionBuilder::BNT, &end);

  // Compile the statements in the loop
  if (body) CompileExpr(builder, body);

  // Compile the re-initialization statements
  builder->AddLabel(&cont);
  if (update) CompileExpr(builder, update);

  // Make the branch back to the loop start.
  builder->AddBranchOp(FunctionBuilder::JMP, &start);

  // Compile the label at the end of the loop
  builder->AddLabel(&end);
}

void MakeBreak(FunctionBuilder* builder, PNode* theNode) {
  // break ::= 'break' [number]

  // Get the number of levels to break from.
  int level = theNode->val - 1;

  // Walk through the list of loops until we get to the proper
  // level to break to.  If the requested break level is greater
  // than the nesting level of the loops, go to the outermost loop.
  Loop* lp;
  for (lp = loopList; level > 0; --level)
    if (lp->next) lp = lp->next;

  builder->AddBranchOp(FunctionBuilder::JMP, lp->end);
}

void MakeBreakIf(FunctionBuilder* builder, PNode* theNode) {
  // breakif ::= 'break' expression [number]

  // Get the number of levels to break from.
  int level = theNode->val - 1;

  // Compile the expression which determines whether or not we break.
  CompileExpr(builder, theNode->first_child());

  // Walk through the list of loops until we get to the proper
  // level to break to.  If the requested break level is greater
  // than the nesting level of the loops, go to the outermost loop.
  Loop* lp;
  for (lp = loopList; level > 0; --level)
    if (lp->next) lp = lp->next;

  builder->AddBranchOp(FunctionBuilder::BNT, lp->end);
}

void MakeContinue(FunctionBuilder* builder, PNode* theNode) {
  // continue ::= 'continue' [number]

  // Get the number of levels to continue at.
  int level = theNode->val - 1;

  // Walk through the list of loops until we get to the proper
  // level to continue at.  If the requested continue level is greater
  // than the nesting level of the loops, go to the outermost loop.
  Loop* lp;
  for (lp = loopList; level > 0; --level)
    if (lp->next) lp = lp->next;

  builder->AddBranchOp(FunctionBuilder::JMP, lp->cont);
}

void MakeContIf(FunctionBuilder* builder, PNode* theNode) {
  // contif ::= 'contif' expression [number]

  // Get the number of levels to continue at.
  int level = theNode->val - 1;

  // Compile the expression which determines whether or not we continue.
  CompileExpr(builder, theNode->first_child());

  // Walk through the list of loops until we get to the proper
  // level to continue at.  If the requested continue level is greater
  // than the nesting level of the loops, go to the outermost loop.
  Loop* lp;
  for (lp = loopList; level > 0; --level)
    if (lp->next) lp = lp->next;

  builder->AddBranchOp(FunctionBuilder::BT, lp->cont);
}
