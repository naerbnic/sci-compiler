#ifndef TOKTYPES_HPP
#define TOKTYPES_HPP

#include <string_view>

#include "scic/symbol.hpp"
#include "scic/symtypes.hpp"

Symbol* LookupTok();
Symbol* GetSymbol();
bool GetNumber(std::string_view);
bool GetNumberOrString(std::string_view);
bool GetString(std::string_view);
bool GetIdent();
bool GetDefineSymbol();
bool IsIdent();
bool IsUndefinedIdent();
keyword_t Keyword();
void GetKeyword(keyword_t);
bool IsVar();
bool IsProc();
bool IsObj();
bool IsNumber();

extern int gSelectorIsVar;

#endif