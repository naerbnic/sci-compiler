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

#include <string>

// this function trims all of the whitespace from the passed string, both front
// and back
void trimstr(char *str);

// this function creates a new string (ala strdup)
char *newStr(const char *str);

// Equivalent of vsprintf, but that returns a std::string instead.
std::string vstringf(const char *fmt, va_list args);

// Equivalent of sprintf, but that returns a std::string instead.
std::string stringf(const char *fmt, ...);

#endif
