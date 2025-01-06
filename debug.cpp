//	debug.cpp	sc playgrammer		8/20/92
//		debug information functions

#if defined(PLAYGRAMMER)

#include "sol.hpp"

#include	"sc.hpp"

#include "error.hpp"
#include "object.hpp"
#include "symtbl.hpp"

static char	fileName[] = "global.smb";

Boolean
ReadDebugFile()
{
	// Read the global debug symbol file

	FILE* fp = fopen(fileName, "rt");
	if (!fp) {
		Warning("Can't open %s", fileName);
		return False;
	}

	char		buf[1024];

	Boolean rc = True;
	while (fgets(buf, sizeof buf, fp)) {
		char delims[] = ", \n";

		char* cp = strtok(buf, delims);
		if (!cp) {
			Error("Missing name in %s", fileName);
			rc = False;
			break;
		}

		Symbol*	s;
		Object*	o;
		if ((s = classSymTbl->lookup(cp)) &&
			 (o = s->obj)) {
			cp = strtok(0, delims);
			if (!cp) {
				Error("Missing file name in %s", fileName);
				rc = False;
				break;
			}

			delete[] o->fullFileName;
			o->fullFileName = newStr(cp);

			cp = strtok(0, delims);
			if (!cp) {
				Error("Missing starting offset in %s", fileName);
				rc = False;
				break;
			}

			o->srcStart = atol(cp);

			cp = strtok(0, delims);
			if (!cp) {
				Error("Missing ending offset in %s", fileName);
				rc = False;
				break;
			}

			o->srcEnd = atol(cp);
		}
	}

	fclose(fp);
	return rc;
}

void
WriteDebugFile()
{
	// Write the global debug symbol file

	FILE* fp = fopen(fileName, "wt");
	if (!fp) {
		Error("Can't open %s", fileName);
		return;
	}

	for (Symbol* s = classSymTbl->firstSym();
		  s;
		  s = classSymTbl->nextSym()) {
		Object* c = s->obj;
		//	do we have any debug info for this class?
		if (c->fullFileName)
			fprintf(fp, "%s, %s, %lu, %lu\n",
					s->name, c->fullFileName, c->srcStart, c->srcEnd);
	}

	if (fclose(fp) == EOF)
		Panic("Error writing %s", fileName);
}

Boolean
GetClassSource(
	char*	className,
	char*	dest)
{
	// copy source text for this class to dest

	Symbol* sym = classSymTbl->lookup(className);
	if (!sym)
		return False;

	Object* c = sym->obj;
	if (!c || !c->fullFileName)
		return False;

	FILE* dfp = fopen(c->fullFileName, "rb");
	if (!dfp)
		return False;

	fsetpos(dfp, &c->srcStart);
	size_t size = size_t(c->srcEnd - c->srcStart) + 1;
	char* buf = New char[size + 1];
	fread(buf, size, 1, dfp);
	buf[size] = '\0';
	strcpy(dest, buf);
	delete[] buf;
	fclose(dfp);

	return True;
}

#endif