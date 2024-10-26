//	output.cpp	sc
//		write binary output files

#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

#include "sol.hpp"

#include "jeff.hpp"

#include "memtype.hpp"

#include "sc.hpp"

#include "error.hpp"
#include "output.hpp"
#include "resource.hpp"

Bool	highByteFirst;

OutputFile::OutputFile(const char* fileName) : 
	fileName(fileName)
{
	if ((fd = open(fileName, OMODE, PMODE)) == -1)
		Panic("Can't open output file %s", fileName);
}

OutputFile::~OutputFile()
{
	close(fd);
}

void
OutputFile::SeekTo(long offset)
{
	lseek(fd, offset, 0);
}

int
OutputFile::Write(const char* str)
{
	int length = strlen(str);
	WriteWord(length);
	Write(str, length);
	return length + sizeof(SCIWord);
}

void
OutputFile::WriteWord(SCIWord w)
{
	// write a word in proper byte order
	//	NOTE:  this code assumes knowledge of the size of SCIWord

	if (highByteFirst)
		// swap high and low bytes
		w = ((w >> 8) & 0xFF) | (w << 8);

	Write(&w, sizeof w);
}

void
OutputFile::WriteByte(ubyte b)
{
	Write(&b, sizeof b);
}

void
OutputFile::Write(const void* mp, size_t len)
{
	//	cast to void* is necessary for WATCOM
	if (write(fd, (void*) mp, len) != len)
		Panic("Error writing %s", fileName);
}

//////////////////////////////////////////////////////////////////////////////

static OutputFile*	OpenObjFile(MemType type, const char* name);
static void				MakeObjFileName(char* dest, MemType type);

void
OpenObjFiles(OutputFile** heapOut, OutputFile** hunkOut)
{
	char	heapName[_MAX_PATH + 1];
	char	hunkName[_MAX_PATH + 1];
	
	//	make the names and delete old objects
	MakeObjFileName(heapName, MemResHeap);
	MakeObjFileName(hunkName, MemResHunk);
	
	//	open the new files
	*heapOut = OpenObjFile(MemResHeap, heapName);
	*hunkOut = OpenObjFile(MemResHunk, hunkName);
}

static void
MakeObjFileName(char* dest, MemType type)
{
	char* resName = ResNameMake(type, script);
	MakeName(dest, outDir, resName, resName);
	unlink(dest);
}

static OutputFile*
OpenObjFile(MemType type, const char* name)
{
	OutputFile* out = New OutputFile(name);

	// Put out the header information.
	ubyte header[2];
	header[0] = (char) type;
	header[1] = 0;
	out->Write(header, sizeof header);

	return out;
}
