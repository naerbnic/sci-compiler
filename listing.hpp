// listing.hpp

#if !defined(LISTING_HPP)
#define LISTING_HPP

void	OpenListFile(const char* sourceFileName);
void	CloseListFile();
void	DeleteListFile();
void	Listing(char*, ...);
void	ListOp(Bool);
void	ListArg(char*, ...);
void	ListAsCode(char*, ...);
void	ListWord(uword);
void	ListByte(ubyte);
void	ListOffset();
void	ListText(char*);
void	ListingNoCRLF(strptr parms, ...);
void	ListSourceLine(int num);

extern Bool	listCode;

#endif
