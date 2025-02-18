#ifndef GLOBAL_COMPILER_HPP
#define GLOBAL_COMPILER_HPP

#include <memory>

#include "scic/codegen/compiler.hpp"

extern std::unique_ptr<CodeGenerator> gSc;

#endif