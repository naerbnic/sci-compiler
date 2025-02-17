#include "scic/parse_context.hpp"

#include <csetjmp>

#include "absl/container/btree_map.h"
#include "scic/class.hpp"
#include "scic/object.hpp"
#include "scic/public.hpp"

jmp_buf gRecoverBuf;
absl::btree_map<int, Class*> gClasses;
int gMaxClassNum = -1;

Object* gCurObj;
Object* gReceiver;
Symbol* gNameSymbol;

PublicList publicList;
int publicMax = -1;