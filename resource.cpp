//	resource.cpp	sc
// 	resource name handling

#include "resource.hpp"

#include <stdio.h>
#include <stdlib.h>

#include "sc.hpp"
#include "sol.hpp"
#include "string.hpp"

const char* ResNameMake(MemType type, int num) {
  // It's gross to have a static buffer in a function, but this is is copying
  // the behavior of the original code.
  static std::string buf;

  //	a quick and dirty way of avoiding bringing in the interpreter's
  //	resource manager just to determine file extensions
  buf = stringf("%u.%s", (SCIUWord)num,
                type == MemResHeap    ? "hep"
                : type == MemResHunk  ? "scr"
                : type == MemResVocab ? "voc"
                                      : "???");

  return buf.c_str();
}
