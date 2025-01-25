//
// fileio
//
// This file contains various file I/O and path manipulation routines.
//
// author: Stephen Nichols
//

#ifndef _FILEIO_HPP_
#define _FILEIO_HPP_

#include <string>
#include <string_view>

//
// This function builds a path name from the provided variables and stores it in
// dest.
//
std::string MakeName(std::string_view dir, std::string_view name,
                     std::string_view ext);

//
// This function looks at the passed string and returns a pointer to the
// string if an extension is found.  Otherwise, it returns a pointer to the
// end of the string.
bool HasExt(std::string_view str);

#endif
