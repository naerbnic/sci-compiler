# CMakeList.txt : CMake project for Script Compiler, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.12)

if (POLICY CMP0025)
    cmake_policy(SET CMP0025 NEW)
endif ()

project ("Script Compiler")

set(CMAKE_CXX_STANDARD 20)

# Add source to this project's executable.
add_executable (sc
getargs.cpp
string.cpp
fileio.cpp
alist.cpp
anode.cpp
asm.cpp
banner.cpp
builtins.cpp
chartype.cpp
class.cpp
compile.cpp
define.cpp
error.cpp
expr.cpp
input.cpp
list.cpp
listing.cpp
loop.cpp
mem.cpp
object.cpp
optimize.cpp
output.cpp
parse.cpp
pnode.cpp
proc.cpp
resource.cpp
selector.cpp
share.cpp
symbol.cpp
symtbl.cpp
text.cpp
token.cpp
toktypes.cpp
update.cpp
sc.cpp
)

set_target_properties(sc PROPERTIES COMPILE_WARNING_AS_ERROR ON)

if(MSVC)
  target_compile_options(sc PRIVATE /W4 /WX)
else()
  target_compile_options(sc PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# TODO: Add tests and install targets if needed.
