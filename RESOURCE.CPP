//	resource.cpp	sc
// 	resource name handling

#include <stdio.h>
#include <stdlib.h>

#include "sol.hpp"

#include	"sc.hpp"

#include	"resource.hpp"

char*
ResNameMake(MemType type, int num)
{
	static char	buf[_MAX_PATH + 1];
	
	//	a quick and dirty way of avoiding bringing in the interpreter's
	//	resource manager just to determine file extensions
	sprintf(buf, "%u.%s", (SCIUWord) num,
		type == MemResHeap ?	"hep" :
		type == MemResHunk ?	"scr" :
		type == MemResVocab ? "voc" :
		"???");

	return buf;
}
