//
// string
//
// This file contains simple string manipulation routines.
//
// author: Stephen Nichols
//

#include <string.h>
#include <ctype.h>
#include "string.hpp"

// this function trims all of the whitespace from the passed string, both front and back
void trimstr ( char *str )
{
    char *endPtr = &str[strlen(str)];
    char *frontPtr = str;

	// the string is empty, return
	if ( endPtr == str )
		return;

	// skip past the null
	endPtr--;

    // scan from the end of the string
    while ( isspace ( *endPtr ) ) {
        *endPtr = 0;
        endPtr--;

        // the entire string is whitespace!
        if ( endPtr == str ) {
            *str = 0;
            return;
        }
    }

	// adjust endPtr to point to the null
	endPtr++;

    // scan from the front of the string
    while ( isspace ( *frontPtr ) ) {
        frontPtr++;

        // we've consumed the whole string!  (should never happen)
        if ( frontPtr == endPtr ) 
            break;
    }

    // move the whole string down
	if ( str != frontPtr ) {
		int length = (int)(endPtr - frontPtr);
	    memmove ( str, frontPtr, length );

		// terminate the string
		str[length] = 0;
	}
}

// this function mimics strdup
char *newStr ( char *str )
{
    if ( str )
        return strdup ( str );

    return NULL;
}
