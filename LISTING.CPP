//	listing.cpp		sc
// 	handle code listing

#include <io.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "sol.hpp"

#include	"jeff.hpp"

#include	"sc.hpp"

#include	"anode.hpp"
#include	"error.hpp"
#include	"listing.hpp"
#include	"opcodes.hpp"

Bool	listCode;

static FILE*	listFile;
static char		listName[_MAX_PATH + 1];
static FILE*	sourceFile;
static int		sourceLineNum;

#define	JUST_OP	0		// Only operator -- no arguments
#define	OP_ARGS	1		// Operator takes arguments
#define	OP_SIZE	2		// Operator takes a size

struct OpStr {
	char*		str;
	UInt16	info;
}  static theOpCodes[] = {
	"bnot",	JUST_OP,
	"add",	JUST_OP,
	"sub",	JUST_OP,
	"mul",	JUST_OP,
	"div",	JUST_OP,
	"mod",	JUST_OP,
	"shr",	JUST_OP,
	"shl",	JUST_OP,
	"xor",	JUST_OP,
	"and",	JUST_OP,
	"or",		JUST_OP,

	"neg",	JUST_OP,
	"not",	JUST_OP,

	"eq?",	JUST_OP,
	"ne?",	JUST_OP,
	"gt?",	JUST_OP,
	"ge?",	JUST_OP,
	"lt?",	JUST_OP,
	"le?",	JUST_OP,

	"ugt?",	JUST_OP,
	"uge?",	JUST_OP,
	"ult?",	JUST_OP,
	"ule?",	JUST_OP,

	"bt",		OP_ARGS | OP_SIZE,
	"bnt",	OP_ARGS | OP_SIZE,
	"jmp",	OP_ARGS | OP_SIZE,

	"ldi",	OP_ARGS | OP_SIZE,
	"push",	JUST_OP,
	"pushi",	OP_ARGS | OP_SIZE,
	"toss",	JUST_OP,
	"dup",	JUST_OP,
	"link",	OP_ARGS | OP_SIZE,

	"call",	OP_ARGS | OP_SIZE,
	"callk",	OP_ARGS | OP_SIZE,
	"callb",	OP_ARGS | OP_SIZE,
	"calle",	OP_ARGS | OP_SIZE,

	"ret",	JUST_OP,

	"send",	JUST_OP,
	"DUMMY",	JUST_OP,
	"DUMMY",	JUST_OP,

	"class",	OP_ARGS | OP_SIZE,
	"DUMMY",	JUST_OP,
	"self",	JUST_OP,
	"super",	OP_ARGS | OP_SIZE,
	"&rest",	OP_ARGS | OP_SIZE,

	"lea",	OP_ARGS | OP_SIZE,
	"selfID",	JUST_OP,
	"DUMMY",	JUST_OP,
	"pprev",	JUST_OP,

	"pToa",	OP_ARGS | OP_SIZE,
	"aTop",	OP_ARGS | OP_SIZE,
	"pTos",	OP_ARGS | OP_SIZE,
	"sTop",	OP_ARGS | OP_SIZE,
	"ipToa",	OP_ARGS | OP_SIZE,
	"dpToa",	OP_ARGS | OP_SIZE,
	"ipTos",	OP_ARGS | OP_SIZE,
	"dpTos",	OP_ARGS | OP_SIZE,

	"lofsa",	OP_ARGS | OP_SIZE,
	"lofss",	OP_ARGS | OP_SIZE,
	"push0", JUST_OP,
	"push1", JUST_OP,
	"push2", JUST_OP,
	"pushSelf", JUST_OP,
};

void
OpenListFile(const char* sourceFileName)
{
	if (!listCode)
		return;

	MakeName(listName, (char *)sourceFileName, (char *)sourceFileName, ".sl");
	if (!(listFile = fopen(listName, "wt")))
		Panic("Can't open %s for listing", listName);

	if (includeDebugInfo) {
		if (!(sourceFile = fopen(sourceFileName, "rt")))
			Panic("Can't open %s for source lines in listing", sourceFileName);
		sourceLineNum = 0;
	}

	Listing("\n\t\t\t\tListing of %s:\t[script %d]\n\n",
				sourceFileName, (SCIUWord) script);
	Listing("LINE/\tOFFSET\tCODE\t\t\t\tNAME");
	Listing("LABEL\t(HEX)\n");
}

void
CloseListFile()
{
	fclose(listFile);
	listFile = 0;
	if (sourceFile) {
		fclose(sourceFile);
		sourceFile = 0;
	}
}

void
DeleteListFile()
{
	unlink(listName);
}

void
Listing(char* parms, ...)
{
	va_list	argPtr;

	if (!listCode || !listFile)
		return;

	va_start(argPtr, parms);
	if (vfprintf(listFile, parms, argPtr) == EOF ||
		 putc('\n', listFile) == EOF)
		Panic("Error writing list file");
	va_end(argPtr);
}

void
ListingNoCRLF(char* parms, ...)
{
	va_list	argPtr;

	if (!listCode || !listFile)
		return;

	va_start(argPtr, parms);

	if (vfprintf(listFile, parms, argPtr) == EOF)
		Panic("Error writing list file");
	va_end(argPtr);
}

void
ListOp(int theOp)
{
	OpStr*	oPtr;
	char*	sp;
	char*	op;
	char		opStr[10];
	Bool		hasArgs;
	Bool		addSize;

	if (!listCode || !listFile)
		return;

	ListOffset();

	op = sp = opStr;
	if (!(theOp & OP_LDST)) {
		oPtr = &theOpCodes[(theOp & ~OP_BYTE) / 2];
		hasArgs = oPtr->info & OP_ARGS;
		addSize = oPtr->info & OP_SIZE;
		if (addSize)
			sp = stradd(sp, oPtr->str);
		else 
			op = oPtr->str;

	} else {
		switch (theOp & OP_TYPE) {
			case OP_LOAD:
				*sp++ = 'l';
				break;
	
			case OP_STORE:
				*sp++ = 's';
				break;
	
			case OP_INC:
				*sp++ = '+';
				break;

			case OP_DEC:
				*sp++ = '-';
				break;
		}

		if (theOp & OP_STACK)
			*sp++ = 's';
		else
			*sp++ = 'a';

		switch (theOp & OP_VAR) {
			case OP_GLOBAL:
				*sp++ = 'g';
				break;
	
			case OP_LOCAL:
				*sp++ = 'l';
				break;
	
			case OP_TMP:
				*sp++ = 't';
				break;
	
			case OP_PARM:
				*sp++ = 'p';
				break;
		}
	
		if (theOp & OP_INDEX)
			*sp++ = 'i';

		addSize = hasArgs = True;
	}

	if (op == opStr)
		*sp = '\0';

	if (hasArgs)
		ListingNoCRLF("%-5s", op);
	else
		Listing("%s", op);
}

void
ListArg(char* parms, ...)
{
	va_list	argPtr;
	char		argStr[80];

	if (!listCode || !listFile)
		return;

	va_start(argPtr, parms);
	argStr[0] = '\t';
	vsprintf(&argStr[1], parms, argPtr);

	Listing(argStr);

	va_end(argPtr);
}

void
ListAsCode(char* parms, ...)
{
	va_list	argPtr;
	char		buf[1024];

	if (!listCode || !listFile)
		return;

	ListOffset();

	va_start(argPtr, parms);
	vsprintf(buf, parms, argPtr);
	Listing(buf);
	va_end(argPtr);
}

void
ListWord(uword w)
{
	if (!listCode || !listFile)
		return;

	ListAsCode("word\t$%x", (SCIUWord) w);
}

void
ListByte(ubyte b)
{
	if (!listCode || !listFile)
		return;

	ListAsCode("byte\t$%x", b);
}

void
ListText(char* s)
{
	char*	l;
	char		line[81];

	ListAsCode("text");

	l = line;
	*l++ = '"';			// start with a quote
	while (1) {
		// Copy from the text until the output line is full.

		while (l < &line[80] && *s != '\0' && *s != '\n') {
			if (*s == '%')
				*l++ = '%';
			*l++ = *s++;
		}

		// If the line is not full, we are done.  Finish with a quote.

		if (l < &line[80]) {
			*l++ = '"';
			*l++ = '\n';
			*l = '\0';
			Listing(line);
			break;
		}

		// Scan back in the text to a word break, then print the line.

		while (l > line && *l != ' ') {
			--l;
			--s;
		}
		*l = '\0';
		++s;				// point past the space
		Listing(line);

		l = line;
	}
}

void
ListOffset()
{
	if (!listCode || !listFile)
		return;

	ListingNoCRLF("\t\t%5x\t", curOfs);
}

void
ListSourceLine(int num)
{
	char buf[512];
	for (; sourceLineNum < num; sourceLineNum++) {
		if ( !fgets(buf, sizeof buf, sourceFile) ) {
			Panic("Can't read source line %d", sourceLineNum);
		}
	}
	ListingNoCRLF("%s", buf);
}

