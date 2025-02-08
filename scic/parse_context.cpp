#include "scic/parse_context.hpp"

#include <csetjmp>
#include <deque>
#include <memory>

#include "scic/class.hpp"
#include "scic/object.hpp"
#include "scic/public.hpp"
#include "scic/varlist.hpp"

jmp_buf gRecoverBuf;
Class* gClasses[MAXCLASSES];
int gMaxClassNum = -1;

Object* gCurObj;
Object* gReceiver;
Symbol* gNameSymbol;

VarList gLocalVars;
VarList gGlobalVars;

std::deque<std::unique_ptr<Public>> publicList;
int publicMax = -1;