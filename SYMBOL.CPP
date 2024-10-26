//	symbol.cpp	sc
// 	symbol class routines for sc

#include "sol.hpp"

#include	"sc.hpp"

#include	"string.hpp"

//#include "char.hpp"
#include	"define.hpp"
#include	"input.hpp"
#include	"object.hpp"
#include	"symbol.hpp"

Symbol::Symbol(const char* name, sym_t type) :

	type(type), name(newStr((char *)name)), next(0), an(0), str(0), lineNum(curLine)
{
}

Symbol::~Symbol()
{
	switch (type) {
		case S_DEFINE:
			delete[] str;
			break;

		case S_EXTERN:
			delete ext;
			break;

		case S_OBJ:
			delete obj;
			break;
		}

	delete[] name;
}
