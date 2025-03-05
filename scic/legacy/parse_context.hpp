#ifndef PARSE_CONTEXT_HPP
#define PARSE_CONTEXT_HPP

#include <csetjmp>

#include "absl/container/btree_map.h"
#include "scic/legacy/class.hpp"
#include "scic/legacy/object.hpp"

#define MAXCLASSES 512  // Maximum number of classes

extern jmp_buf gRecoverBuf;
extern absl::btree_map<int, Class*> gClasses;
extern int gMaxClassNum;

extern Object* gCurObj;
extern Symbol* gNameSymbol;
extern Object* gReceiver;

#endif