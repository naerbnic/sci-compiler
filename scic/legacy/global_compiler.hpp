#ifndef GLOBAL_COMPILER_HPP
#define GLOBAL_COMPILER_HPP

#include <memory>

#include "scic/codegen/code_generator.hpp"

extern std::unique_ptr<codegen::CodeGenerator> gSc;

#endif