#ifndef PARSE_CONTEXT_HPP
#define PARSE_CONTEXT_HPP

#include <csetjmp>
#include <deque>
#include <memory>

#include "absl/container/btree_map.h"
#include "scic/class.hpp"
#include "scic/object.hpp"
#include "scic/public.hpp"
#include "scic/varlist.hpp"

#define MAXCLASSES 512  // Maximum number of classes

class ParseContext {
 public:
  ParseContext();

  jmp_buf recoverBuf;
  absl::btree_map<int, Class*> classes;
  int maxClassNum;

  Object* curObj;
  Object* receiver;
  Symbol* nameSymbol;

  VarList globalVars;
  VarList localVars;

  std::deque<std::unique_ptr<Public>> publicList;
  int publicMax;
};

extern ParseContext gParseContext;

#endif