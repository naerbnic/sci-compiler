//
// fileio
//
// This file contains various file I/O and path manipulation routines.
//
// author: Stephen Nichols
//

#ifndef _FILEIO_HPP_
#define _FILEIO_HPP_

//
// This function builds a path name from the provided variables and stores it in dest.
// 
void MakeName ( char *dest, char *dir, char *name, char *ext );

//
// This function looks at the passed string and returns a pointer to the string if an
// extension is found.  Otherwise, it returns a pointer to the end of the string.
char *_ExtPtr ( char *str );

#endif

