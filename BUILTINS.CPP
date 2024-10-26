//	builtins.cpp
// 	code to install the keywords, etc., in the symbol table.

#include "sol.hpp"

#include	"sc.hpp"

#include	"builtins.hpp"
#include	"symtbl.hpp"

struct BuiltIn {
	strptr	name;
	sym_t		type;
	int		val;
} builtIns[] = {
	"include",		   S_KEYWORD,	K_INCLUDE,
	"public",		   S_KEYWORD,	K_PUBLIC,
	"extern",		   S_KEYWORD,	K_EXTERN,
	"global",		   S_KEYWORD,	K_GLOBAL,
	"local",			   S_KEYWORD,	K_LOCAL,
	"define",		   S_KEYWORD,	K_DEFINE,
	"enum",			   S_KEYWORD,	K_ENUM,
	"procedure",	   S_KEYWORD,	K_PROC,
	"selectors",	   S_KEYWORD,	K_SELECT,
	"class-def",	   S_KEYWORD,	K_CLASSDEF,
	"classdef",		   S_KEYWORD,	K_CLASSDEF,
	"script#",		   S_KEYWORD,	K_SCRIPTNUM,
	"class#",		   S_KEYWORD,	K_CLASSNUM,
	"super#",		   S_KEYWORD,	K_SUPER,
	"class",			   S_KEYWORD,	K_CLASS,
	"properties",	   S_KEYWORD,	K_PROPLIST,
	"methods",		   S_KEYWORD,	K_METHODLIST,
	"method",		   S_KEYWORD,	K_METHOD,
	"instance",		   S_KEYWORD,	K_INSTANCE,
	"of",				   S_KEYWORD,	K_OF,
	"kindof",		   S_KEYWORD,	K_OF,
	"kind-of",		   S_KEYWORD,	K_OF,
	"&tmp",			   S_KEYWORD,	K_TMP,
	"return",		   S_KEYWORD,	K_RETURN,
	"break",			   S_KEYWORD,	K_BREAK,
	"breakif",		   S_KEYWORD,	K_BREAKIF,
	"continue",		   S_KEYWORD,	K_CONT,
	"contif",		   S_KEYWORD,	K_CONTIF,
	"while",			   S_KEYWORD,	K_WHILE,
	"repeat",		   S_KEYWORD,	K_REPEAT,
	"for",			   S_KEYWORD,	K_FOR,
	"if",				   S_KEYWORD,	K_IF,
	"else",			   S_KEYWORD,	K_ELSE,
	"cond",			   S_KEYWORD,	K_COND,
	"switch",		   S_KEYWORD,	K_SWITCH,
	"++",				   S_KEYWORD,	K_INC,
	"--",				   S_KEYWORD,	K_DEC,
	"&rest",			   S_KEYWORD,	K_REST,
	"+",				   S_NARY,		N_PLUS,
	"*",				   S_NARY,		N_MUL,
	"^",				   S_NARY,		N_BITXOR,
	"&",				   S_NARY,		N_BITAND,
	"|",				   S_NARY,		N_BITOR,
	"-",				   S_BINARY,	B_MINUS,
	"/",				   S_BINARY,	B_DIV,
	"mod",			   S_BINARY,	B_MOD,
	"<<",				   S_BINARY,	B_SLEFT,
	">>",				   S_BINARY,	B_SRIGHT,
	"=",				   S_ASSIGN,	A_EQ,
	"+=",				   S_ASSIGN,	A_PLUS,
	"*=",				   S_ASSIGN,	A_MUL,
	"-=",				   S_ASSIGN,	A_MINUS,
	"/=",				   S_ASSIGN,	A_DIV,
	"<<=",			   S_ASSIGN,	A_SLEFT,
	">>=",			   S_ASSIGN,	A_SRIGHT,
	"^=",				   S_ASSIGN,	A_XOR,
	"&=",				   S_ASSIGN,	A_AND,
	"|=",				   S_ASSIGN,	A_OR,
	"~",				   S_UNARY,		U_BNOT,
	"not",			   S_UNARY,		U_NOT,
	"neg",			   S_UNARY,		U_NEG,
	">",				   S_COMP,		C_GT,
	">=",				   S_COMP,		C_GE,
	"<",				   S_COMP,		C_LT,
	"<=",				   S_COMP,		C_LE,
	"u>",				   S_COMP,		C_UGT,
	"u>=",			   S_COMP,		C_UGE,
	"u<",				   S_COMP,		C_ULT,
	"u<=",			   S_COMP,		C_ULE,
	"==",				   S_COMP,		C_EQ,
	"!=",				   S_COMP,		C_NE,
	"and",			   S_NARY,	   N_AND,
	"or",				   S_NARY,  	N_OR,
	"TRUE",			   S_NUM,		1,
	"FALSE",			   S_NUM,		0,
	"argc",			   S_PARM,		0,
	"file#",			   S_KEYWORD,	K_FILE,
	"switchto",		   S_KEYWORD,	K_SWITCHTO,
	NULL
};

void
InstallBuiltIns()
{
	// Install the builtin symbol table.

	for (BuiltIn* bp = builtIns; bp->name; ++bp) {
		Symbol* sp = syms.installGlobal(bp->name, bp->type);
		sp->val = bp->val;
	}
}
