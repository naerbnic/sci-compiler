//	sc.hpp
//		standard header for all SC files

#ifndef SC_HPP
#define SC_HPP

// Modes for opening files.
#define	OMODE	(int) 		(O_BINARY | O_CREAT | O_RDWR | O_TRUNC)
#define	PMODE	(int) 		(S_IREAD | S_IWRITE)

#define	REQUIRED				1
#define	OPTIONAL				0

#define	UNDEFINED			0
#define	DEFINED				1

typedef char*				strptr;
typedef unsigned char	ubyte;
typedef unsigned short	uword;

class FixupList;
class CodeList;

struct Compiler {
	Compiler();
	~Compiler();

	FixupList*	heapList;
	CodeList*	hunkList;
} extern * sc;

extern Bool	includeDebugInfo;
extern char	outDir[];
extern char	progName[];
extern int	script;
extern Bool	verbose;

#endif
