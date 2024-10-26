//	input.hpp		sc
//		definitions for input source structure

#if !defined(INPUT_HPP)
#define INPUT_HPP

#if !defined(__STDIO_H)
#include	"stdio.h"
#endif

struct InputSource {
	InputSource();
	InputSource(char* fileName, int lineNum = 0);
	virtual ~InputSource() {}

	InputSource& operator=(InputSource&);

	virtual Bool	incrementPastNewLine(char*&) = 0;
	virtual Bool	endInputLine() = 0;

	InputSource*	next;
	strptr			fileName;
	int				lineNum;
	strptr			ptr;
};

struct InputFile : InputSource {
	InputFile(FILE*, char* name);
	~InputFile();

	Bool	incrementPastNewLine(char*&);
	Bool	endInputLine();

	FILE*				file;
	strptr			fullFileName;
	fpos_t			lineStart;
};

struct InputString : InputSource {
	InputString();
	InputString(char* str);

	InputString& operator=(InputString&);

	Bool	incrementPastNewLine(char*&);
	Bool	endInputLine();
};

Bool				CloseInputSource();
void				FreeIncludePath();
Bool				GetNewInputLine();
InputSource*	OpenFileAsInput(strptr, Bool);
void				SetIncludePath();
void				SetStringInput(strptr);
void				SetInputToCurrentLine();
void				RestoreInput();

void				SetTokenStart();
void				SetParseStart();
fpos_t   		GetParseStart();
fpos_t			GetParsePos();
fpos_t			GetTokenEnd();
void				SetTokenEnd();

class StrList;

extern char				curFile[];
extern int				curLine;
extern InputSource*	curSourceFile;
extern StrList*		includePath;
extern InputSource*	is;
extern InputSource*	theFile;

#endif

