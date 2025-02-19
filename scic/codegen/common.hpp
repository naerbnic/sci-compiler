#ifndef CODEGEN_COMMON_HPP
#define CODEGEN_COMMON_HPP

//	data types for interfacing to the SCI language, in which all values
//	are 16 bits
#include <cstdint>
using SCIWord = std::int16_t;
using SCIUWord = std::uint16_t;

inline constexpr int KERNEL = -1;  // Module # of kernel

#endif