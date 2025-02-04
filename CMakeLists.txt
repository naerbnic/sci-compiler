# CMakeList.txt : CMake project for Script Compiler, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.30)
include(FetchContent)

if (POLICY CMP0025)
    cmake_policy(SET CMP0025 NEW)
endif ()

project ("Script Compiler")

set(CMAKE_CXX_STANDARD 20)

option(ABSL_PROPAGATE_CXX_STD
  "Use CMake C++ standard meta features (e.g. cxx_std_14) that propagate to targets that link to Abseil"
  ON) 

FetchContent_Declare(absl
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG origin/lts_2025_01_27)
FetchContent_Declare(argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse.git
  GIT_TAG origin/master)
FetchContent_MakeAvailable(absl argparse)

# Add source to this project's executable.
add_executable (sc
src/config.cpp
src/alist.cpp
src/anode.cpp
src/asm.cpp
src/banner.cpp
src/builtins.cpp
src/chartype.cpp
src/class.cpp
src/compile.cpp
src/define.cpp
src/error.cpp
src/expr.cpp
src/input.cpp
src/list.cpp
src/listing.cpp
src/loop.cpp
src/object.cpp
src/optimize.cpp
src/output.cpp
src/platform.cpp
src/parse.cpp
src/pnode.cpp
src/proc.cpp
src/resource.cpp
src/selector.cpp
src/share.cpp
src/symbol.cpp
src/symtbl.cpp
src/text.cpp
src/token.cpp
src/toktypes.cpp
src/update.cpp
src/sc.cpp
src/io/buffer.cpp
)

target_include_directories(sc PRIVATE src/)

set_target_properties(sc PROPERTIES COMPILE_WARNING_AS_ERROR ON)
target_link_libraries(sc
  absl::strings
  absl::str_format
  absl::flat_hash_map
  absl::btree
  absl::base
  absl::debugging
  absl::failure_signal_handler
  argparse
)

if(MSVC)
  target_compile_options(sc PRIVATE /W4 /WX)
else()
  target_compile_options(sc PRIVATE
    -Wall
    -Wpedantic
    # Disable gcc-compat, as ABSL uses it with appropriately flagged code
    -Wno-gcc-compat
    -fsanitize=address
    -Werror)
  target_link_options(sc PRIVATE
    -fsanitize=address
  )
endif()

# TODO: Add tests and install targets if needed.
