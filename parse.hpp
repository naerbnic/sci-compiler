//	parse.hpp	sc
// 	definitions for parse nodes.

#ifndef PARSE_HPP
#define PARSE_HPP

#include <setjmp.h>

#ifndef SYMBOL_HPP
#include "symbol.hpp"
#endif

// Parse node types.  The same as symbol types, but with some additions.
enum pn_t {
	PN_END = 128,		// end of input
	PN_KEYWORD,			// keyword
	PN_DEFINE,			// definition
	PN_IDENT,			// unknown identifier
	PN_LABEL,			// label
	PN_GLOBAL,			// global variable
	PN_LOCAL,			// local variable
	PN_TMP,				// temporary variable
	PN_PARM,				// parameter
	PN_PROC,				// procedure
	PN_EXTERN,			// external procedure/object
	PN_ASSIGN,			// assignment
	PN_NARY,				// nary arithmetic operator
	PN_BINARY,			// binary operator
	PN_UNARY,			// unary arithmetic operator
	PN_COMP,				// comparison operator
	PN_NUM,				// number
	PN_STRING,			// string
	PN_CLASS,			// class
	PN_OBJ,				// object
	PN_SELECT,			// object selector
	PN_WORD,				// word

	PN_METHOD,			// method
	PN_KMETHOD,			// kernal method
	PN_LPROP,			// property
	PN_ELIST,			// expression list
	PN_EXPR,				// expression
	PN_INDEX,			// indexed variable
	PN_SEND,				// send to object
	PN_CALL,
	PN_LINK,
	PN_FOR,
	PN_WHILE,
	PN_REPEAT,
	PN_BREAK,
	PN_BREAKIF,
	PN_CONT,
	PN_CONTIF,
	PN_IF,
	PN_COND,
	PN_SWITCH,
	PN_ELSE,
	PN_INCDEC,
	PN_RETURN,
	PN_SUPER,
	PN_REST,
	PN_PROP,
	PN_METH,
	PN_MSG,
	PN_SWITCHTO
};

struct PNode {
	// Node for building a parse tree.

	PNode(pn_t t);
	~PNode();

	PNode*	addChild(PNode* node);

	PNode*	next;		// pointer to next in sibling list
	PNode*	child;	// pointer to head of children list
	Symbol*	sym;		// symbol associated with node
	int		val;		// node value
	pn_t		type;		// type of node
	int		lineNum;	//	line number in current source file
};

//	parse.cpp
Bool		Parse();
void		Include();
Bool		OpenBlock();
Bool		CloseBlock();

//	expr.cpp
Bool		ExprList(PNode*, Bool);
Bool		Expression(PNode*, Bool);

//	proc.cpp
void		Procedure();
PNode*	CallDef(sym_t);

extern Bool		inParmList;
extern jmp_buf	recoverBuf;

#endif

