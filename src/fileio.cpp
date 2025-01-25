//
// fileio
//
// This file contains various file I/O and path manipulation routines.
//
// author: Stephen Nichols
//

#include "fileio.hpp"

#include <filesystem>
#include <string_view>

//
// This function builds a path name from the provided variables and stores it in
// dest.
//
std::string MakeName(std::string_view dir, std::string_view name,
                     std::string_view ext) {
  std::string result;

  // let's put the directory
  result.append(dir);

  // let's put the name if any different
  if (dir != name) {
    //
    // If the dir has no trailing separator and the name has no leading
    // separator and the dir is not a drive selector, add a separator.
    //
    char sep = std::filesystem::path::preferred_separator;
    if (!dir.empty() && (dir.back() != sep) && (dir.back() != ':') &&
        (name.front() != sep)) {
      result.push_back(sep);
    }
    result.append(name);
  }

  // let's put the extension if any different
  if (name != ext) result.append(ext);

  return result;
}

//
// This function looks at the passed string and returns a pointer to the string
// if an extension is found.  Otherwise, it returns a pointer to the end of the
// string.
//
bool HasExt(std::string_view str) {
  return str.rfind('.') != std::string_view::npos;
}
