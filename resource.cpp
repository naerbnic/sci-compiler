//	resource.cpp	sc
// 	resource name handling

#include "resource.hpp"

#include <stdio.h>
#include <stdlib.h>

#include "sc.hpp"
#include "sol.hpp"

char* ResNameMake(MemType type, int num) {
  static char buf[_MAX_PATH + 1];

  //	a quick and dirty way of avoiding bringing in the interpreter's
  //	resource manager just to determine file extensions
  sprintf(buf, "%u.%s", (SCIUWord)num,
          type == MemResHeap    ? "hep"
          : type == MemResHunk  ? "scr"
          : type == MemResVocab ? "voc"
                                : "???");

  return buf;
}
