//	error.hpp

#ifndef ERROR_HPP
#define ERROR_HPP

#include "sc.hpp"

void EarlyEnd();
void Error(strptr parms, ...);
[[noreturn]] void Fatal(strptr parms, ...);
void Info(strptr parms, ...);
void output(strptr fmt, ...);
void Severe(strptr parms, ...);
void Warning(strptr parms, ...);
[[noreturn]] void Panic(strptr parms, ...);

extern int errors;
extern int warnings;

#endif
