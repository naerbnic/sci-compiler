//	debug.hpp	sc playgrammer		8/20/92
//		interface to debug information functions

#if !defined(DEBUG_HPP)
#define DEBUG_HPP

#if defined(PLAYGRAMMER)

Boolean	ReadDebugFile();
void		WriteDebugFile();
Boolean	GetClassSource(char* className, char* dest);

#endif

#endif
