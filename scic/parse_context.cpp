#include "scic/parse_context.hpp"

ParseContext gParseContext;

ParseContext::ParseContext()
    : maxClassNum(-1),
      curObj(nullptr),
      receiver(nullptr),
      nameSymbol(nullptr),
      publicMax(-1) {}