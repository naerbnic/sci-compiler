//	token.hpp	sc

#if !defined(TOKEN_HPP)
#define TOKEN_HPP

#if !defined(SYMBOL_HPP)
#include	"symbol.hpp"
#endif

//	token.cpp
void			GetToken();
Bool			NextToken();
Bool			NewToken();
void			UnGetTok();
void			GetRest(Bool error = False);
Bool			GetNewLine();

//	toktypes.cpp
Symbol*		LookupTok();
Bool			GetSymbol();
Bool			GetNumber(strptr);
Bool			GetNumberOrString(strptr);
Bool			GetString(strptr);
Bool			GetIdent();
Bool			GetDefineSymbol();
Bool			IsIdent();
keyword_t	Keyword();
void			GetKeyword(keyword_t);
Bool			IsVar();
Bool			IsProc();
Bool			IsObj();
Bool			IsNumber();

#define	symObj	tokSym.obj
#define	symType	tokSym.type
#define	symVal	tokSym.val

const int MaxTokenLen	= 2048;

extern int		nestedCondCompile;
extern int		selectorIsVar;
extern char		symStr[];
extern Symbol	tokSym;

#endif
