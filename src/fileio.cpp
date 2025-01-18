//
// fileio
//
// This file contains various file I/O and path manipulation routines.
//
// author: Stephen Nichols
//

#include "fileio.hpp"

#include <string_view>

#include "string.hpp"

//
// This function builds a path name from the provided variables and stores it in
// dest.
//
void MakeName(char *dest, const char *dir, const char *name, const char *ext) {
  *dest = 0;

  // let's put the directory
  strcat(dest, dir);

  // let's put the name if any different
  if (dir != name) {
    //
    // If the dir has no trailing backslash and the name has no leading
    // backslash and the dir is not a drive selector, add a backslash.
    //
    int dirLen = strlen(dir);

    if (dirLen && (dir[dirLen - 1] != '\\') && (dir[dirLen - 1] != ':') &&
        (*name != '\\'))
      strcat(dest, "\\");

    strcat(dest, name);
  }

  // let's put the extension if any different
  if (name != ext) strcat(dest, ext);
}

std::string MakeName(std::string_view dir, std::string_view name,
                     std::string_view ext) {
  std::string result;

  result.append(dir);
  if (dir != name) {
    if (!dir.empty() && (dir.back() != '\\') && (dir.back() != ':') &&
        (name.front() != '\\')) {
      result.append("\\");
    }
    result.append(name);
  }

  if (name != ext) result.append(ext);

  return result;
}

//
// This function looks at the passed string and returns a pointer to the string
// if an extension is found.  Otherwise, it returns a pointer to the end of the
// string.
//
const char *_ExtPtr(const char *str) {
  if (strchr(str, '.')) return str;

  return &str[strlen(str)];
}
