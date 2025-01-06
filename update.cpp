//	update.cpp		sc
// 	update the database of class and selector information

#include <stdlib.h>
#include <string.h>

#include "sol.hpp"

#include	"jeff.hpp"

#include "vocab.hpp"

#include	"sc.hpp"

#include	"error.hpp"
#include	"input.hpp"
#include	"debug.hpp"
#include	"object.hpp"
#include	"output.hpp"
#include	"resource.hpp"
#include	"symtbl.hpp"
#include	"token.hpp"
#include	"update.hpp"

Bool	classAdded;
Bool	selectorAdded;
char	outDir[_MAX_PATH + 1];
Bool	writeOffsets;

static ubyte	resHdr[]	= { MemResVocab, 0 };

static void	WriteSelector();
static void	WriteClassDefs();
static void	WriteClasses();
static void	WriteSelectorVocab();
static void	PrintSubClasses(Class* op, int level, FILE* fp);

void
UpdateDataBase()
{
	if (selectorAdded) {
		WriteSelector();
		WriteSelectorVocab();
	}

	if (classAdded) {
		WriteClassDefs();
		WriteClasses();
	}

#if defined(PLAYGRAMMER)
	WriteDebugFile();
#endif

	selectorAdded = classAdded = False;
}

void
WriteClassTbl()
{
	// Write the file 'classtbl'.  This is an array, indexed by class number,
	// with two words for each class.  The first word is space to store the
	// class ID when the class is loaded.  The second word contains the script
	// number in which the class resides.

	static	ubyte	resID[2] = {MemResVocab, 0};

	_Packed struct ClassTblEntry {
		SCIUWord	objID;
		SCIUWord	scriptNum;
	};

	// Allocate storage for the class table.
	ClassTblEntry* classTbl = New ClassTblEntry[maxClassNum + 1];

	// Now walk through the class symbol table, entering the script
	// number of each class in its proper place in the table.
	int index;
	for (Symbol* sym = syms.classSymTbl->firstSym();
			sym;
			sym = syms.classSymTbl->nextSym()) {
		if (sym->obj->num != -1) {
			classTbl[sym->obj->num].objID = 0;
			classTbl[sym->obj->num].scriptNum = SCIUWord(sym->obj->script);
		}
	}

	// Write the table out.
	strptr name = ResNameMake(MemResVocab, CLASSTBL_VOCAB);
   char fileName[_MAX_PATH + 1];
	MakeName(fileName, outDir, name, name);
	OutputFile out(fileName);
	out.Write(resID, 2);
	for (index = 0 ; index < maxClassNum + 1 ; ++index) {
		out.WriteWord(classTbl[index].objID);
		out.WriteWord(classTbl[index].scriptNum);
	}

	delete[] classTbl;
}

void
WritePropOffsets()
{
	// Read the file "offsets.txt" and write out "offsets", a file with the
	// offsets (in words) of properties in given classes.  This allows for
	// considerably faster execution in the kernel.

	Object*		cp;
	int			offset;
	Selector*	sel;
	Symbol*		theSym;
	char			fileName[_MAX_PATH + 1];
	char*			name;

	theFile = OpenFileAsInput("offsets.txt", True);

	name = ResNameMake(MemResVocab, PROPOFS_VOCAB);
	MakeName(fileName, outDir, name, name);
	OutputFile out(fileName);

	// Write out the resource header (this will be a vocabulary resource).
	out.Write(resHdr, sizeof resHdr);

	while (NewToken()) {
		theSym = syms.lookup(symStr);
		if (theSym->type != S_CLASS) {
			Error("Not a class: %s", symStr);
			GetToken();
			continue;
		}
		cp = theSym->obj;
		if (!LookupTok() || !(sel = cp->findSelector(&tokSym))) {
			Error("Not a selector for class %s: %s", cp->sym->name, symStr);
			continue;
		}

		offset = sel->ofs / 2;
		out.WriteWord(offset);
	}
}

static void
WriteSelector()
{
	FILE*	fp;

	if (!(fp = fopen("selector", "w")))
		Panic("Can't open 'selector' for output.");
	fseek(fp, 0L, SEEK_SET);

	fprintf(fp, "(selectors\n");
	for (Symbol *sp = syms.selectorSymTbl->firstSym() ;
		  sp;
		  sp = syms.selectorSymTbl->nextSym())
		fprintf(fp, "\t%-20s %d\n", sp->name, sp->val);

	fprintf(fp, ")\n");

	if (fclose(fp) == EOF)
		Panic("Error writing selector file");
}

static void
WriteClassDefs()
{
	FILE*	fp;
	if (!(fp = fopen("classdef", "w")))
		Panic("Can't open 'classdef' for output.");

	int classNum = -1;
	for (Class* cp = NextClass(classNum); cp; cp = NextClass(classNum)) {
		classNum = cp->num;
		if (cp->num == -1)
			continue;			// This is RootObj, defined by compiler.

		fprintf(fp,
			 "(classdef %s\n"
			 "	script# %d\n"
			 "	class# %d\n"
			 "	super# %d\n"
			 "	file# \"%s\"\n\n",
				cp->sym->name,
				(SCIUWord) cp->script,
				(SCIUWord) cp->num,
				(SCIUWord) cp->super,
				cp->file
			 );

		// Get a pointer to the class' super-class.
		Class* sp = FindClass(cp->findSelector("-super-")->val);

		// Write out any New properties or properties which differ in
		// value from the superclass.
		fprintf(fp, "\t(properties\n");
		Selector* tp;
		for (tp = cp->selectors; tp; tp = tp->next) {
			if (IsProperty(tp) && sp->selectorDiffers(tp))
				fprintf(fp, "\t\t%s %d\n", tp->sym->name, tp->val);
		}
		fprintf(fp, "\t)\n\n");

		// Write out any New methods or methods which have been redefined.
		fprintf(fp, "\t(methods\n");
		for (tp = cp->selectors ; tp; tp = tp->next)
			if (IsMethod(tp) && sp->selectorDiffers(tp))
				fprintf(fp, "\t\t%s\n", tp->sym->name);
		fprintf(fp, "\t)\n");

		fprintf(fp, ")\n\n\n");
	}

	if (fclose(fp) == EOF)
		Panic("Error writing classdef");
}

static void
WriteClasses()
{
	FILE	*fp;

	// Open the file 'classes', on which this heirarchy is printed.
	if (!(fp = fopen("classes", "w")))
		Panic("Can't open 'classes' for output.");

	// Print the classes in heirarchical order.
	PrintSubClasses(classes[0], 0, fp);

	// Close the file.
	if (fclose(fp) == EOF)
   	Panic("Error writing 'classes'");
}

static void
PrintSubClasses(
	Class*	sp,
	int		level,
	FILE*		fp)
{
	// Print out this class' information.
	fprintf(fp, "%.*s%-*s;%s\n",
				2*level, "               ",
				20-2*level, sp->sym->name,
				sp->file
			 );

	// Print information about this class' subclasses.
	++level;
	for (Class* cp = sp->subClasses; cp; cp = cp->nextSibling)
		PrintSubClasses(cp, level, fp);
}

static void
WriteSelectorVocab()
{
	static char		badSelMsg[] = "BAD SELECTOR";

	Symbol*		sp;
	SCIUWord*	tbl;
	size_t		ofs;
	uint			tblLen;
	int			i;
	char			fileName[_MAX_PATH + 1];
	char*			resName;

	resName = ResNameMake(MemResVocab, SELECTOR_VOCAB);
	MakeName(fileName, outDir, resName, resName);

	// Compute the size of the table needed to hold offsets to all selectors,
	// allocate the table, and initialize it to point to the byte following
	// the table so that un-implemented selector values will have a string.
	tblLen = sizeof(SCIUWord) * (maxSelector + 2);
	tbl = New SCIUWord[tblLen / sizeof *tbl];
	ofs = tblLen;
	*tbl = SCIUWord(maxSelector);
	for (i = 1 ; i <= maxSelector + 1 ; ++i)
		tbl[i] = SCIUWord(ofs);

	OutputFile out(fileName);

	// Write out the resource header (this will be a vocabulary resource).
	out.Write(resHdr, sizeof resHdr);

	// Seek to the beginning of the string area of the file and write the
	// bad selector string.
	out.SeekTo(tblLen + sizeof resHdr);
	ofs += out.Write(badSelMsg);

	// Now write out the names of all the other selectors and put their
	// offsets into the table.
	for (sp = syms.selectorSymTbl->firstSym() ;
		  sp;
		  sp = syms.selectorSymTbl->nextSym()) {
		tbl[sp->val + 1] = SCIUWord(ofs);
		ofs += out.Write(sp->name);
	}

	// Seek back to the table's position in the file and write it out.
	out.SeekTo(sizeof resHdr);
	for (i = 0; i < tblLen / sizeof *tbl; i++)
		out.WriteWord(tbl[i]);

	delete[] tbl;
}
