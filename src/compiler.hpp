// compiler.hpp
//		compiler-specific information:  WATCOM 32-bit

#ifndef COMPILER_HPP
#define COMPILER_HPP

#include <cstdint>

//	data types for external data (files or OS data structures)
//	each contain the number of bits indicated

//	this should be in a WATCOM header file, but it's not, so...
extern "C" char** _argv;

//	heap checking assertion
#define HEAPCHK assert(_heapchk() <= _HEAPEMPTY);

//	file functions
#define ffblk find_t
#define ff_name name
#define ff_attrib attrib
#define ff_fsize size
#define ff_ftime wr_time

#define findfirst(path, struc, attrs) _dos_findfirst(path, attrs, struc)
#define findnext(struc) _dos_findnext(struc)

#define fnmerge _makepath
#define fnsplit _splitpath

struct ftime {
  unsigned ft_tsec : 5;  /* Two second interval */
  unsigned ft_min : 6;   /* Minutes */
  unsigned ft_hour : 5;  /* Hours */
  unsigned ft_day : 5;   /* Days */
  unsigned ft_month : 4; /* Months */
  unsigned ft_year : 7;  /* Year */
};

#endif
