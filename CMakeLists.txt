# CMakeList.txt : CMake project for Script Compiler, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("Script Compiler")

# Add source to this project's executable.
add_executable (sc
new.cpp
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

# TODO: Add tests and install targets if needed.
