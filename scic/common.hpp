// Common definitions used across the SCI compiler codebase.

#ifndef COMMON_HPP
#define COMMON_HPP

#include <cstddef>
#include <cstdint>

enum class RequiredState {
  OPTIONAL = 0,
  REQUIRED = 1,
};

using RequiredState::OPTIONAL;
using RequiredState::REQUIRED;

inline constexpr std::size_t UNDEFINED = 0;
inline constexpr std::size_t DEFINED = 1;

//	data types for interfacing to the SCI language, in which all values
//	are 16 bits
using SCIWord = std::int16_t;
using SCIUWord = std::uint16_t;

#endif