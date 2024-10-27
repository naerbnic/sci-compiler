//	loop.cpp
// 	loop code generation

#include "sol.hpp"

#include	"sc.hpp"

#include	"anode.hpp"
#include	"compile.hpp"
#include	"opcodes.hpp"
#include	"parse.hpp"
#include	"symbol.hpp"

enum LoopType { LOOP_FOR, LOOP_WHILE, LOOP_REPEAT };

struct Loop {
	Loop(LoopType t, Symbol* c, Symbol* e);
	~Loop();

	Loop*			next;
	LoopType		type;
	ANode*		start;	// address of start of the loop
	Symbol*		cont;		// symbol for continue address
	Symbol*		end;		// symbol for the end of the loop
};

// 'loopList' is a stack of the currently active loops.  We can scan it
// to support such things as (break n) and (continue n).
Loop*	loopList;

Loop::Loop(
	LoopType	t,
	Symbol*	c,
	Symbol*	e) :

	type(t), cont(c), end(e), next(loopList), start(New ANLabel)
{
	// Add this loop to the loop list.
	loopList = this;
}

Loop::~Loop()
{
	// Remove this loop from the loop list.
	loopList = next;
}

void
MakeWhile(
	PNode*		theNode)
// while ::= 'while' expression statement*
{
	Symbol	cont;
	Symbol	end;
	Loop 		lp(LOOP_WHILE, &cont, &end);

	cont.loc = lp.start;

	// Compile the conditional expression controlling the loop,
	// and its corresponding branch.
	theNode = theNode->child;
	Compile(theNode);
	MakeBranch(op_bnt, 0, &end);

	// Compile the statements in the loop
	theNode = theNode->next;
	if (theNode)
		Compile(theNode);

	// Make the branch back to the loop start.
	MakeBranch(op_jmp, lp.start, 0);

	// Compile the label at the end of the loop
	MakeLabel(&end);
}

void
MakeRepeat(
	PNode* theNode)
{
	// forever ::= 'forever' statement+

	Symbol	cont;
	Symbol	end;
	Loop		lp(LOOP_REPEAT, &cont, &end);

	cont.loc = lp.start;

	// Compile the loop's statements.
	if (theNode->child)
		Compile(theNode->child);

	// Make the branch back to the start of the loop.
	MakeBranch(op_jmp, lp.start, 0);

	// Make the target label for breaks.
	MakeLabel(&end);
}

void
MakeFor(
	PNode* theNode)
{
	// for ::=	'for' '(' statement* ')'
	//			expression
	//			'(' statement* ')'
	//			statement*

	// Make the initialization statements.
	theNode = theNode->child;
	if (theNode)
		Compile(theNode);

	// Make the label at the start of the loop
	Symbol	end;
	Symbol	cont;
	Loop		lp(LOOP_FOR, &cont, &end);

	// Compile the conditional expression controlling the loop,
	// and its corresponding branch.
	theNode = theNode->next;
	if (theNode)
		Compile(theNode);
	MakeBranch(op_bnt, 0, &end);

	// Compile the statements in the loop
	if (theNode->next && theNode->next->next)
		Compile(theNode->next->next);

	// Compile the re-initialization statements
	MakeLabel(&cont);
	theNode = theNode->next;
	if (theNode)
		Compile(theNode);

	// Make the branch back to the loop start.
	MakeBranch(op_jmp, lp.start, 0);

	// Compile the label at the end of the loop
	MakeLabel(&end);
}

void
MakeBreak(
	PNode* theNode)
{
	// break ::= 'break' [number]

	// Get the number of levels to break from.
	int level = theNode->val - 1;

	// Walk through the list of loops until we get to the proper
	// level to break to.  If the requested break level is greater
	// than the nesting level of the loops, go to the outermost loop.
	Loop* lp;
	for (lp = loopList; level > 0; --level)
		if (lp->next)
			lp = lp->next;

	MakeBranch(op_jmp, 0, lp->end);
}

void
MakeBreakIf(
	PNode* theNode)
{
	// breakif ::= 'break' expression [number]

	// Get the number of levels to break from.
	int level = theNode->val - 1;

	// Compile the expression which determines whether or not we break.
	Compile(theNode->child);

	// Walk through the list of loops until we get to the proper
	// level to break to.  If the requested break level is greater
	// than the nesting level of the loops, go to the outermost loop.
	Loop* lp;
	for (lp = loopList; level > 0; --level)
		if (lp->next)
			lp = lp->next;

	MakeBranch(op_bt, 0, lp->end);
}

void
MakeContinue(
	PNode* theNode)
{
	// continue ::= 'continue' [number]

	// Get the number of levels to continue at.
	int level = theNode->val - 1;

	// Walk through the list of loops until we get to the proper
	// level to continue at.  If the requested continue level is greater
	// than the nesting level of the loops, go to the outermost loop.
	Loop* lp;
	for (lp = loopList; level > 0; --level)
		if (lp->next)
			lp = lp->next;

	if (lp->type == LOOP_FOR)
		MakeBranch(op_jmp, 0, lp->cont);
	else
		MakeBranch(op_jmp, lp->start, 0);
}

void
MakeContIf(
	PNode* theNode)
{
	// contif ::= 'contif' expression [number]

	// Get the number of levels to continue at.
	int level = theNode->val - 1;

	// Compile the expression which determines whether or not we continue.
	Compile(theNode->child);

	// Walk through the list of loops until we get to the proper
	// level to continue at.  If the requested continue level is greater
	// than the nesting level of the loops, go to the outermost loop.
	Loop* lp;
	for (lp = loopList; level > 0; --level)
		if (lp->next)
			lp = lp->next;

	if (lp->type == LOOP_FOR)
		MakeBranch(op_bt, 0, lp->cont);
	else
		MakeBranch(op_bt, lp->start, 0);
}
