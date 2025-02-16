# Semantic Analysis Passes

At this point in the processing of an SCI script, our program is fully parsed
as a sequence of `parsers::sci::Item` objects. We will perform multiple passes
in order to ensure the script is sane, and know all of the entities that are
being defined in the script.

## Scopes

There are logically three scopes that we are concerned about while analyzing
the script:

- Global: Things that are available to the entire codebase. This includes the
set of all classes, the set of global variables, and any public members in other
modules. Each of these add names to the global name scope.
- Module: Things that are defined within our current module. This includes local
variables and procedures.
- Local: Within a method or procedure, the local properties, parameters, and
temporaries are in scope.

## Pass 1: Top Level Symbol Gathering and Sanity Checking (`initial_pass`)

Before this pass, we know nothing about the actual symbol contents of the
program, nor on the global environment. This pass gathers the Global and
Module level symbols for the module, as well as check that there are no
redundancies or conflicts between different items in the script.

A script must have:

- Exactly one `script#` item. Number must be positive.
- Any number of `global_decl` blocks, to define the global-level variables
  available. If there are multiple blocks that cover the same variables, names
  must be consistent, and the same global index should have only one name
  associated with it (can be a warning rather than an error).
- Up to one `global` or `local` block. `global` blocks are only allowed in
  script 0. If there are `global_decl` and `global` blocks, they must be
  compatible with each other.
- Any number of `extern` blocks. Multiple of these must be consistent for the
  same names.
- Any number of `selectors` blocks. Multiple of these must be consistent for the
  same names.
- Any number of `classdecl` blocks. Each class decl must define a different
  class. This will be placed in the global scope.
- Any number of `class`, `instance`, and `procedure` definitions. All of these
  names must be distinct.
- Up to one `public` block. Names are not resolved.
