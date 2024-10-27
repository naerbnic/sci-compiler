//	input.cpp	sc
// 	input routines

#include <dos.h>
#include <stdlib.h>

#include "sol.hpp"

#include	"jeff.hpp"

#include	"sc.hpp"

#include	"error.hpp"
#include	"fileio.hpp"
#include	"input.hpp"
#include	"string.hpp"
#include	"text.hpp"
#include	"token.hpp"

char				curFile[_MAX_PATH + 1];
int				curLine;
InputSource*	curSourceFile;
StrList*			includePath;
InputSource*	is;
InputSource*	theFile;

static char		inputLine[512];

static void		SetInputSource(InputSource*);

InputSource::InputSource() :
	fileName(0), lineNum(0)
{
	tokSym.str = 0;
	curLine = lineNum;
}

InputSource::InputSource(
	char*		fileName,
	int		lineNum) :

	fileName(fileName), lineNum(lineNum)
{
	tokSym.str = symStr;
	curLine = lineNum;
}

InputSource&
InputSource::operator=(
	InputSource& s)
{
	fileName = s.fileName;
	lineNum = s.lineNum;
	curLine = lineNum;
   return *this;
}

InputFile::InputFile(
	FILE*	fp,
	char*	name) :

	InputSource(newStr(name)),
	file(fp)
{
	strcpy(curFile, fileName);
}

InputFile::~InputFile()
{
	delete[] fileName;
	fclose(file);
}

Bool
InputFile::incrementPastNewLine(
	char*&	ip)
{
	if (GetNewLine()) {
		ip = is->ptr;
		return True;
	}
	return False;
}

Bool
InputFile::endInputLine()
{
	return GetNewLine();
}

InputString::InputString(
	char*	str) :

	InputSource(curFile, curLine)
{
	ptr = str;
}

InputString::InputString()
{
	ptr = 0;
}

InputString&
InputString::operator =(
	InputString& s)
{
	InputSource::operator=(s);
	ptr = s.ptr;
	return *this;
}

Bool
InputString::endInputLine()
{
	return CloseInputSource();
}

Bool
InputString::incrementPastNewLine(
	char*&	ip)
{
	++ip;
	return True;
}

InputSource*
OpenFileAsInput(
	strptr	fileName,
	Bool		required)
{
	FILE*				file;
	InputSource*	theFile;
	char				newName[_MAX_PATH + 1];
	StrList*			ip;

	// Try to open the file.  If we can't, try opening it in each of
	// the directories in the include path.
	ip = includePath;
	*newName = '\0';
	for (file = fopen(fileName, "r"); !file && ip; ip = ip->next) {
		MakeName(newName, ip->str, fileName, fileName);
		file = fopen(newName, "r");
	}

	if (!file)
		if (required)
			Panic("Can't open %s", fileName);
		else
			return 0;

    // SLN - following code serves no purpose, removed
//	if (!*newName)
//		strcpy(newName, fileName);
//	fullyQualify(newName);

	theFile = New InputFile(file, fileName);

#if defined(PLAYGRAMMER)
	theFile->fullFileName = newStr(newName);
#endif
	SetInputSource(theFile);
	GetNewLine();

	return theFile;
}

Bool
CloseInputSource()
{
	// Close the current input source.  If the source is a file, this involves
	// closing the file.  Remove the source from the chain and free its memory.

	InputSource*	ois;

	if (ois = is) {
		InputSource* next = is->next;
		delete ois;
		is = next;
	}

	if (is) {
		strcpy(curFile, is->fileName);
		curLine = is->lineNum;
	}

	return (Bool) is;
}

void
SetStringInput(
	strptr str)
{
	SetInputSource(New InputString(str));
}

Bool
GetNewInputLine()
{
	// Read a New line in from the current input file.  If we're at end of
	// file, close the file, shifting input to the next source in the queue.

	while	(is) {
#if defined(PLAYGRAMMER)
		fgetpos(is->file, &is->lineStart);
#endif
		if (is->ptr = fgets(inputLine, sizeof inputLine, ((InputFile*) is)->file))
			break;
		CloseInputSource();
	}

	if (is) {
		++is->lineNum;
		++curLine;
	}

	return (Bool) is;
}

void
SetIncludePath()
{
	strptr	t;
	strptr	p;
	char		path[128];
	StrList*	sn;
	StrList*	last;

	if (!(t = getenv("SINCLUDE")))
		return;

	// Successively copy each element of the path into 'path',
	// and add it to the includePath chain.
	for (p = path, last = 0; *t; p = path) {
		// Scan to end of environment or a ';'
		for ( ; *t && *t != ';' ; ++t, ++p)
			*p = (char) (*t == '\\' ? '/' :* t);

		// Make sure that the path terminates with either a '/' or ':'
		if (p[-1] != '/' && p[-1] != ':')
			*p++ = '/';
		*p = '\0';

		// Point past a ';' (but not the end of the environment).
		if (*t)
			++t;

		// Now allocate a node to keep this path on in the includePath chain
		// and link it into the list.
		sn = New StrList;
		if (!last)
			last = includePath = sn;
		else {
			last->next = sn;
			last = sn;
		}

		// Save the pathname and hang it off the node.
		sn->str = newStr(path);
	}
}

void
FreeIncludePath()
{
	// Free the memory alloted to the include path.

	StrList*	sn;
	StrList* tmp;

	for (sn = includePath; sn; ) {
		tmp = sn->next;
		delete[] sn->str;
		delete sn;
		sn = tmp;
	}
	includePath = 0;
}

static InputSource*	saveIs;
static InputString*	curLineInput;

void
SetInputToCurrentLine()
{
	//	set the current input line as the input source

	saveIs = is;
	curLineInput = new InputString(inputLine);
	is = curLineInput;
}

void
RestoreInput()
{
	if (saveIs) {
		is = saveIs;
		saveIs = 0;
	}
}

static void
SetInputSource(
	InputSource* nis)
{
	// Link a New input source (pointed to by 'nis') in at the head of the input
	// source queue.

	nis->next = is;
	is = nis;
}

////////////////////////////////////////////////////////////////////////////

static fpos_t	startToken;
static fpos_t	endToken;
static fpos_t	startParse;

void
SetTokenStart()
{
	startToken = GetParsePos();
}

void
SetParseStart()
{
	startParse = startToken;
}

fpos_t
GetParseStart()
{
	return startParse;
}

fpos_t
GetParsePos()
{
	return ((InputFile*) is)->lineStart + (long) (is->ptr - inputLine);
}

fpos_t
GetTokenEnd()
{
	return endToken;
}

void
SetTokenEnd()
{
	endToken = GetParsePos() - 1;
}


