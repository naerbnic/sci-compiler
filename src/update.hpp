//	update.hpp

#ifndef UPDATE_HPP
#define UPDATE_HPP

void UpdateDataBase();
void WriteClassTbl();
void WritePropOffsets();

extern bool classAdded;
extern char outDir[];
extern bool selectorAdded;
extern bool writeOffsets;

#endif
