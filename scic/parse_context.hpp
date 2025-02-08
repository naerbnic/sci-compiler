#ifndef PARSE_CONTEXT_HPP
#define PARSE_CONTEXT_HPP

#include <csetjmp>

#include "scic/class.hpp"
#include "scic/define.hpp"
#include "scic/object.hpp"
#include "scic/varlist.hpp"

#define MAXCLASSES 512  // Maximum number of classes

extern jmp_buf gRecoverBuf;
extern Class* gClasses[];
extern int gMaxClassNum;

extern Object* gCurObj;
extern Symbol* gNameSymbol;
extern Object* gReceiver;

extern VarList gGlobalVars;
extern VarList gLocalVars;

#endif