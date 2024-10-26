//	sc.cpp		sc
// 	the Script language compiler

#include <stdlib.h>
#include <io.h>

#include "sol.hpp"

#include	"jeff.hpp"
#include	"string.hpp"
#include    "getargs.hpp"

#include	"sc.hpp"

#include	"asm.hpp"
#include	"banner.hpp"
#include	"builtins.hpp"
#include	"compile.hpp"
#include	"debug.hpp"
#include	"define.hpp"
#include	"error.hpp"
#include	"input.hpp"
#include	"listing.hpp"
#include	"mem.hpp"
#include	"object.hpp"
#include	"opcodes.hpp"
#include	"output.hpp"
#include	"parse.hpp"
#include	"resource.hpp"
#include	"share.hpp"
#include	"symtbl.hpp"
#include	"text.hpp"
#include	"token.hpp"
#include	"update.hpp"

Bool			includeDebugInfo;
Compiler*	sc;
int			script;
Bool			verbose;

static int	totalErrors;

static void	CompileFile(strptr);
static void	ShowInfo();
static void	InstallCommandLineDefine(char*);

//	used by getargs
static strptr	outDirPtr;
char				usageStr[] = "file_spec [-switches]";
Arg				switches[] = {
	'a', GA_BOOL, (int *) &abortIfLocked,
			"abort compile if database locked",
	'd', GA_BOOL, (int *) &includeDebugInfo,
			"include debug info",
	'D', GA_PROC, (int *) InstallCommandLineDefine,
			"command line define (e.g. -DMAC or -DMAC=1)",
	'g', GA_INT, (int *) &maxVars,
			"maximum number of global or local variables",
	'l', GA_BOOL, (int *) &listCode,
			"generate a code listing",
	'm', GA_BOOL, (int *) &writeMemSizes,
			"write memory allocation statistics",
	'n', GA_BOOL, (int *) &noAutoName,
			"no auto-naming of objects",
	'o', GA_STR, (int *) &outDirPtr,
			"set output directory",
	'O', GA_BOOL, (int *) &writeOffsets,
			"output the 'offsets' file",
	's', GA_BOOL, (int *) &showSelectors,
			"show forward-referenced selectors",
	'u', GA_BOOL, (int *) &dontLock,
			"don't lock class database",
	'v', GA_BOOL, (int *) &verbose,
			"verbose output",
	'w', GA_BOOL, (int *) &highByteFirst,
			"output words high-byte first (for M68000)",
	'z', GA_BOOL, (int *) &noOptimize,
			"turn off optimization",
	0, 0, 0, 0
};

#if !defined(WINDOWS)

Compiler::Compiler()
{
	hunkList = New CodeList;
	heapList = New FixupList;
}

Compiler::~Compiler()
{
	delete hunkList;
   delete heapList;
}

static void
deleteCompiler()
{
	delete sc;
}

int
main(
	int		argc,
	strptr*	argv)
{
	strptr*	ap;
	strptr	op;
	strptr	extPtr;
	int		outLen;
	char		fileName[_MAX_PATH + 1];

	sc = New Compiler;
	atexit(deleteCompiler);

	output(banner);

	if ( getargs ( argc, argv ) == 1 )
		ShowUsage();

	exargs(&argc, &argv);

#if 0
	if (writeMemSizes)
		atexit(WriteMemSizes);
#endif

	// See if the first file to be compiled exists.  If not, exit.
	extPtr = _ExtPtr(argv[1]);
	MakeName(fileName, argv[1], argv[1], (*extPtr)? extPtr : ".sc");
	if (access(fileName, 0) == -1)
		Panic("Can't find %s", fileName);

	// Make sure that any output directory is terminated with a '/'.
	if (outDirPtr) {
		strcpy(outDir, outDirPtr);
		outLen = strlen(outDir);
		op = &outDir[outLen - 1];	/* point to last char of outDir */
		if (*op != '\\' && *op != '/' && *op != ':') {
			*++op = '/';
			*++op = '\0';
		}
	}

	// Set the include path.
	SetIncludePath();
	
	// Install the built-in symbols then read in and install
	// the definitions.  Lock the class database so that we're the only
	// one updating it.
	InstallBuiltIns();
	InstallObjects();
	Lock();
	errors = warnings = 0;
	theFile = OpenFileAsInput("selector", True);
	Parse();
	if (access("classdef", 0) == 0) {
		theFile = OpenFileAsInput("classdef", True);
		Parse();
	}

#if defined(PLAYGRAMMER)
	ReadDebugFile();
#endif

	theFile = OpenFileAsInput("system.sh", True);
	Parse();

	theFile = OpenFileAsInput("game.sh", False);
	if (theFile)
		Parse();

	totalErrors += errors;

	// Compile the files.
	for (ap = &argv[1]; *ap; ++ap) {
		CompileFile(*ap);
    }

	// Write out the class table and unlock the class database.
	WriteClassTbl();
	if (writeOffsets)
		WritePropOffsets();
	Unlock();

	FreeIncludePath();

	return totalErrors != 0;
}

#endif

static void
CompileFile(
	strptr	fileName)
{
	char	sourceFileName[_MAX_PATH + 1];
	char*	extPtr;

	// Do some initialization.
	script = -1;
	errors = warnings = 0;
	InitPublics();
	text.init();

	// Delete any free symbol tables.
	syms.delFreeTbls();


	// Look up the symbol for 'name', as it will be used in object.c.
	nameSymbol = syms.selectorSymTbl->lookup("name");

	// Open the source file.
	extPtr = _ExtPtr(fileName);
	MakeName(sourceFileName, fileName, fileName, *extPtr ? extPtr : ".sc");
	strlwr(sourceFileName);

	output("%s\n", sourceFileName);
	theFile = OpenFileAsInput(sourceFileName, True);
	curSourceFile = theFile;

	// Parse the file (don't lock the symbol tables), then assemble it.
	syms.moduleSymTbl = syms.add(ST_MEDIUM);
	Parse();
	MakeText();							// Add text to the assembly code
	if (script == -1)
		Error("No script number specified.  Can't write output files.");
	else {
		OpenListFile(sourceFileName);
		Assemble();
	}
	totalErrors += errors;

	// Write out class/selector information only if there have been no
	// errors to this point.
	if (!totalErrors)
		UpdateDataBase();

	// Show information.
	ShowInfo();
	if (listCode)
		CloseListFile();
	else
		DeleteListFile();

	// Delete any free symbol tables.
	syms.delFreeTbls();

#if !defined(WINDOWS) && !defined(__386__)
	if (writeMemSizes)
		MemDisplay();
	HEAPCHK
#endif
}

static void
ShowInfo()
{
	if (errors)
		output("\t%d error%s.\n", errors, errors == 1 ? "" : "s");
	else
		output("\tNo errors.\n");
}

static void
InstallCommandLineDefine(
	char*	str)
{
	char*	token;

	if (!(token = strtok(str, "=")))
		Panic("-D flag used without symbol to define");

	if (syms.lookup(token))
		Panic("'%s' has already been defined", token);

	Symbol* sym = syms.installGlobal(token, S_DEFINE);
	token = strtok(0, "");
	sym->str = newStr(token ? token : "1");
}
