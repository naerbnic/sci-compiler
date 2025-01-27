//
// string
//
// This file contains simple string manipulation routines.
//
// author: Stephen Nichols
//

#include "string.hpp"

#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#include <cstdarg>
#include <ranges>
#include <stdexcept>
#include <string>
#include <algorithm>

// this function trims all of the whitespace from the passed string, both front
// and back
void trimstr(char *str) {
  char *endPtr = &str[strlen(str)];
  char *frontPtr = str;

  // the string is empty, return
  if (endPtr == str) return;

  // skip past the null
  endPtr--;

  // scan from the end of the string
  while (isspace(*endPtr)) {
    *endPtr = 0;
    endPtr--;

    // the entire string is whitespace!
    if (endPtr == str) {
      *str = 0;
      return;
    }
  }

  // adjust endPtr to point to the null
  endPtr++;

  // scan from the front of the string
  while (isspace(*frontPtr)) {
    frontPtr++;

    // we've consumed the whole string!  (should never happen)
    if (frontPtr == endPtr) break;
  }

  // move the whole string down
  if (str != frontPtr) {
    int length = (int)(endPtr - frontPtr);
    memmove(str, frontPtr, length);

    // terminate the string
    str[length] = 0;
  }
}

// this function mimics strdup
char *newStr(std::string_view str) {
  if (!str.data()) return nullptr;

  // We use malloc here instead of new, as strdup documents that they can be
  // freed with free.

  char *newStr = (char *)malloc(str.size() + 1);
  std::copy(str.begin(), str.end(), newStr);
  newStr[str.size()] = 0;
  return newStr;
}

std::string vstringf(const char *fmt, va_list args) {
  va_list args2;
  va_copy(args2, args);

  int size = vsnprintf(nullptr, 0, fmt, args2);
  va_end(args2);
  if (size < 0) {
    throw std::runtime_error("vsnprintf failed");
  }

  std::string result;

  if (size > 0) {
    result.resize(size);
    vsnprintf(&result[0], size + 1, fmt, args);
  }

  return result;
}

std::string stringf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  std::string result = vstringf(fmt, args);
  va_end(args);
  return result;
}

void strlwr(std::string &str) {
  std::transform(str.begin(), str.end(), str.begin(), std::tolower);
}
