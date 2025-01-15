//	token.hpp	sc

#if !defined(TOKEN_HPP)
#define TOKEN_HPP

#if !defined(SYMBOL_HPP)
#include "symbol.hpp"
#endif

//	token.cpp
void GetToken();
bool NextToken();
bool NewToken();
void UnGetTok();
void GetRest(bool error = false);
bool GetNewLine();

//	toktypes.cpp
Symbol* LookupTok();
bool GetSymbol();
bool GetNumber(strptr);
bool GetNumberOrString(strptr);
bool GetString(strptr);
bool GetIdent();
bool GetDefineSymbol();
bool IsIdent();
keyword_t Keyword();
void GetKeyword(keyword_t);
bool IsVar();
bool IsProc();
bool IsObj();
bool IsNumber();

#define symObj tokSym.obj
#define symType tokSym.type
#define symVal tokSym.val

const int MaxTokenLen = 2048;

extern int nestedCondCompile;
extern int selectorIsVar;
extern char symStr[];
extern Symbol tokSym;

#endif
