//	resource.cpp	sc
// 	resource name handling

#include "scic/resource.hpp"

#include "absl/strings/str_format.h"
#include "scic/sol.hpp"

std::string ResNameMake(MemType type, int num) {
  //	a quick and dirty way of avoiding bringing in the interpreter's
  //	resource manager just to determine file extensions
  return absl::StrFormat("%u.%s", (SCIUWord)num,
                         type == MemResHeap    ? "hep"
                         : type == MemResHunk  ? "scr"
                         : type == MemResVocab ? "voc"
                                               : "???");
}
