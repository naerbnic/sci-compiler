#include "scic/parse_context.hpp"

#include <csetjmp>

#include "scic/class.hpp"
#include "scic/object.hpp"
#include "scic/varlist.hpp"

jmp_buf gRecoverBuf;
Class* gClasses[MAXCLASSES];
int gMaxClassNum = -1;

Object* gCurObj;
Object* gReceiver;
Symbol* gNameSymbol;

VarList gLocalVars;
VarList gGlobalVars;