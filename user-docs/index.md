---
title: User Guide
layout: default
nav_order: 2
description: "User guide on using scinc"
---

# `scinc` User Guide

{: .warning}
> We are working on making the user experience better for the compiler, including for instructions on how to run from the command line to build your source. Excuse our dust.

## Differences between `scinc` dialect and SCICompanion dialect scripts

Because `scinc` is based on an earlier SCI Script compiler, there are some differences between the languages supported. Fortunately *most* of the syntax is the same, so code should be able to be easily adapted. For instance, these are compatible:

- `script#` definitions
- Class definitions
- Procedure definitions
- All expressions

For the actual changes, there are three major categories: Global Include definitions, syntax changes, and unsupported items.

### Global Include Declarations

In order to allow scripts to compile without having all of the source code to a game available, we have to declare the program structure in a way that can be used from compiled scripts. To do this, we introduce the concept of **global includes**. These are header files with special declarations in them that describe these program structures and the names to use to reference them. This includes:

- Global Variables
- Kernel Functions
- Selectors
- Script Publics
- Classes

When running `scinc`, you can list global include files that contain these declarations, which will be applied to all of the compiled scripts.

Some entities are both declared in the global includes and defined the scripts being compiled. For instance, a script may define publics that have also been defined in the global includes. This does not cause a conflict as long as the declaration is compatbile with the definition, meaning that the module defines all of the publics that were declared with the same index and the same name.

{: .todo}
> Give the set of all global include declarations

### Syntax Changes

A small number of items are different because the older syntax `scinc` is based on parsed it different. If possible, compatibility will be added with the SCICompanion dialect as development continues.

#### Variables

In SCICompanion, locals are declared as a sequence of names:

```common_lisp
(local
  foo
  bar = 3
  [baz 4] = [1 2 3 4]
  [quux 3] = ["a" "b" "c"]
)
```

This implicitly numbers the locals in increasing order. In `scinc`, these need
to be annotated with the indexes of the variable:

```common_lisp
(local
  foo 0
  bar 1 = 3
  [baz 4] 2 = [1 2 3 4]
  [quux 3] 6 = ["a" "b" "c"]
)
```

Note that `quux` has index 8, as the variable `baz` takes up four slots with the array.

In addition, in the root script (i.e. script with number 0), the variables have to be declared as `global` instead of `local`.

```common_lisp
(script# 0)
(global
  gEgo 0
  ; ...
)
```

Globals will be checked against any `globaldecl` global include definitions.

### Unsupported Items

These are items that are supported in SCICompanion, but not in `scinc`. Again, this may change as we try to improve compatibility

#### `use` Items

In SCICompanion, you can import symbols from another script using the `use` item:

```common_lisp
(use Main)
```

Because `scinc` defines *all* public methods that are used in the global includes files, we do not direclty support `use` items. We recommend adding a global includes file that provides `externs` for all public items from the compiled scripts.
