//
// string
//
// This file contains simple string manipulation routines.
//
// author: Stephen Nichols
//

#ifndef _STRING_HPP_
#define _STRING_HPP_

#include <string.h>

// this function trims all of the whitespace from the passed string, both front
// and back
void trimstr(char *str);

// this function creates a new string (ala strdup)
char *newStr(char *str);

#endif
