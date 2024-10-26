//	update.hpp

#ifndef UPDATE_HPP
#define UPDATE_HPP

void	UpdateDataBase();
void	WriteClassTbl();
void	WritePropOffsets();

extern Bool	classAdded;
extern char	outDir[];
extern Bool	selectorAdded;
extern Bool	writeOffsets;

#endif

