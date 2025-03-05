//	resource.cpp	sc
// 	resource name handling

#include "scic/legacy/resource.hpp"

#include <string>

#include "absl/strings/str_format.h"
#include "scic/codegen/common.hpp"
#include "scic/legacy/memtype.hpp"

std::string ResNameMake(MemType type, int num) {
  //	a quick and dirty way of avoiding bringing in the interpreter's
  //	resource manager just to determine file extensions
  return absl::StrFormat("%u.%s", (codegen::SCIUWord)num,
                         type == MemResHeap    ? "hep"
                         : type == MemResHunk  ? "scr"
                         : type == MemResVocab ? "voc"
                                               : "???");
}
