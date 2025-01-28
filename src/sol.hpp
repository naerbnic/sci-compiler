//	sol.hpp	5/25/93
//		standard header for all Sierra On-Line C++ programs

#ifndef SOL_HPP
#define SOL_HPP

//	for size_t
#include <stddef.h>
#include <stdio.h>
#include <string_view>
#include <string.h>

//	establish compiler-dependent types and other information
#include "compiler.hpp"

//	abbreviations
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint32_t;
typedef unsigned long ulong;

//	data types for interfacing to the SCI language, in which all values
//	are 16 bits
typedef int16_t SCIWord;
typedef uint16_t SCIUWord;

#if !defined(_TV_VERSION) && !defined(__TCOLLECT_H)
const bool True = 1;
const bool False = 0;
#endif

template <class T, class S>
inline S Max(S a, T b) {
  return a > b ? a : b;
}

template <class T, class S>
inline S Min(S a, T b) {
  return a < b ? a : b;
}

//	calculate the number of elements in an array whose size is known
#define NEls(array) (sizeof(array) / sizeof *(array))

//	Each implementation should define its own AssertFail().
//	Normally, it will simply call the msgMgr, but we give it its own
// function 	so anyone can use assert() without having to drag MSG.HPP
// around.
int AssertFail(std::string_view file, int line, std::string_view expression);

// Sufficient MAX_PATH for all platforms

#define MAX_PATH 1024

#endif
