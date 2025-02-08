//	parse.hpp	sc
// 	definitions for parse nodes.

#ifndef PARSE_HPP
#define PARSE_HPP

#include <csetjmp>

bool Parse();
void Include();
bool OpenBlock();
bool CloseBlock();

extern jmp_buf gRecoverBuf;

#endif
