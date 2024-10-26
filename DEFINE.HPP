// define.hpp		sc
//		definitions for define.cpp.

#if !defined(DEFINE_HPP)
#define DEFINE_HPP

#if !defined(SYMBOL_HPP)
#include	"symbol.hpp"
#endif

enum	VarType {
	VAR_NONE,
	VAR_GLOBAL,
	VAR_LOCAL,
};

struct Var {
	// Variable definition.
	Var() : type((sym_t) VAR_NONE), value(0) {}

	sym_t		type;
	int		value;
};

struct VarList {
	// Variable block definition.
	VarList() : size(0), fixups(0), type(VAR_NONE), values(0) {}
	
	void	kill();

	int		size;			// number of words allocated for variables
	int		fixups;		// number of fixups in this variable list
	VarType	type;			// what type of variables are these
	Var*		values;		// pointer to block of initial values
};

struct Public {
	// Node for public/export definition lists.

	Public(Symbol* s = 0) : next(0), sym(s), script(0), entry(0)  {}

	Public*	next;
	Symbol*	sym;			// pointer to the relevant symbol
	int		script;		// script number
	uint		entry;		// index in dispatch table
};

void		Define();
void		Enum();
void		Global();
void		Local();
void		DoPublic();
Symbol*	FindPublic(int);
void		Extern();
void		InitPublics();
void		Definition();

extern VarList	globalVars;
extern VarList localVars;
extern int		maxVars;

#endif

