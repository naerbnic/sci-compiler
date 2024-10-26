//	error.hpp

#if !defined(ERROR_HPP)
#define ERROR_HPP

void	EarlyEnd();
void	Error(strptr parms, ...);
void	Fatal(strptr parms, ...);
void	Info(strptr parms, ...);
void	output(strptr fmt, ...);
void	Severe(strptr parms, ...);
void	Warning(strptr parms, ...);
void	Panic(strptr parms, ...);

extern int	errors;
extern int	warnings;

#endif

