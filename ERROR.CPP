//	error.cpp	sc
// 	error message routines for sc

#include <io.h>
#include <stdarg.h>
#include <stdlib.h>

#include "sol.hpp"

#include	"sc.hpp"

#include	"error.hpp"
#include	"input.hpp"
#include	"listing.hpp"
#include	"share.hpp"
#include	"symbol.hpp"
#include	"token.hpp"

int	errors;
int	warnings;

static void	beep();

static char	buf[MaxTokenLen + 100];

void
output(
	strptr fmt,
	...)
{
	va_list args;

	va_start(args, fmt);
	vprintf(fmt, args);
	fflush(stdout);
	va_end(args);

	if (!isatty(fileno(stdout)) && isatty(fileno(stderr))) {
		va_start(args, fmt);
		vfprintf(stderr, fmt, args);
		va_end(args);
	}
}

void
Info(
	strptr parms,
	...)
{
	va_list	argPtr;

	sprintf(buf, "Info: %s, line %d\n\t", curFile, curLine);
	output(buf);
	Listing(buf);

	va_start(argPtr, parms);
	vsprintf(buf, parms, argPtr);
	va_end(argPtr);
	output(buf);
	Listing(buf);

	output("\n");
	Listing("\n");
}

void
Warning(
	strptr parms,
	...)
{
	va_list	argPtr;

	++warnings;

	sprintf(buf, "Warning: %s, line %d\n\t", curFile, curLine);
	output(buf);
	Listing(buf);

	va_start(argPtr, parms);
	vsprintf(buf, parms, argPtr);
	va_end(argPtr);
	output(buf);
	Listing(buf);

	output("\n");
	Listing("\n");

	// Beep on first error/warning.

	if (warnings + errors == 1)
		beep();
}

void
Error(
	strptr parms,
	...)
{
	va_list	argPtr;

	++errors;

	sprintf(buf, "Error: %s, line %d\n\t", curFile, curLine);
	output(buf);
	Listing(buf);

	va_start(argPtr, parms);
	vsprintf(buf, parms, argPtr);
	va_end(argPtr);
	output(buf);
	Listing(buf);

	output("\n");
	Listing("\n");

	if (!CloseP(symType))
		GetRest(True);
	else
		UnGetTok();

	// Beep on first error/warning.

	if (warnings + errors == 1)
		beep();
}

void
Severe(
	strptr parms,
	...)
{
	va_list	argPtr;

	++errors;

	sprintf(buf, "Error: %s, line %d\n\t", curFile, curLine);
	output(buf);
	Listing(buf);

	va_start(argPtr, parms);
	vsprintf(buf, parms, argPtr);
	va_end(argPtr);
	output(buf);
	Listing(buf);

	output("\n");
	Listing("\n");

	if (!CloseP(symType))
		GetRest(True);
	else
		UnGetTok();

	// Beep on first error/warning.
	
	if (warnings + errors == 1)
		beep();
}

void
Fatal(
	strptr parms,
	...)
{
	va_list	argPtr;

	output("Fatal: %s, line %d\n\t", curFile, curLine);
	va_start(argPtr, parms);
	vsprintf(buf, parms, argPtr);
	output(buf);
	va_end(argPtr);
	output("\n");

	Listing("Fatal: %s, line %d\n\t", curFile, curLine);
	Listing(parms);
	Listing("\n");

	beep();

	CloseListFile();
	Unlock();
	exit(3);
}

void
Panic(
	strptr parms,
	...)
{
	va_list	argPtr;
	char		buf[200];

	output("Fatal: ");
	va_start(argPtr, parms);
	vsprintf(buf, parms, argPtr);
	output(buf);
	va_end(argPtr);
	output("\n");

	beep();

	CloseListFile();
	Unlock();
	exit(3);
}

void
EarlyEnd()
{
	Fatal("Unexpected end of input.");
}

static void
beep()
{
	putc('\a', stderr);
}

int
AssertFail(char* file, int line, char* expression)
{
	printf("Assertion failed in %s(%d): %s\n", file, line, expression);
	abort();

	// Return value is only useful for the way this function is used in
	// assert() macro under BC++.  However, WATCOM knows it's senseless.
	return 0;
}

