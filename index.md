---
title: Home
layout: home
nav_order: 1
description: "The SCI New Compiler is a new command-line tool implementation of the Sierra Creative Interpreter script compiler."
---

# `scinc`: The SCI New Compiler
{: .fs-9 }

A command-line compiler tool for compiling Sierra Creative Interpreter Scripts
{: .fs-6 .fw-300 }

[Get Started](#getting-started){: .btn .btn-primary .fs-5 .mb-4 .mb-md-0 .mr-2 }
[GitHub Repo][github-repo]{: .btn .fs-5 .mb-4 .mb-md-0 }

---

The `scinc` tool (pronounced *"sync"*) is a command-line compiler for the Sierra Creative Interpreter Script language. The output of the compiler can be used to build new games or patch existing games run with the SCI engine, such
as those used by [ScummVM].

{: .warning }
> `scinc` is currently in a **very** alpha state. We have used it to compile a few scripts, and believe it to have done so correctly, but it is highly likely that you will run into bugs. You have been warned.

## What is the SCI Engine?

The SCI Engine was a game engine developed by Sierra Entertainment. Many of their best known games were developed using this engine, including:

- King's Quest (4, 5, 6)
- Space Quest (4, 5, 6)
- Gabriel Knight (1, 2)
- Quest for Glory (3, 4)
- And quite a bit more...

## What is SCI Script?

SCI Script is the language used by Sierra to implement games that used the SCI engine. The language is primarily an object-oriented language that uses message passing to communicate between the different objects in the game. It allows pieces of code to be loaded and unloaded from memory to keep a small memory profile (in the days where 16 MB of RAM required special drivers to operate).

## Why make a compiler for SCI Script?

There is still a niche but devoted following of these classic Sierra games, both for the games themselves and the aesthetics of that era. There have been several projects that have attempted to modify the old games for various purposes, and even examples of projects that have used the SCI Engine to create brand new games entirely.

## Aren't there already compilers for SCI Script?

The most popular compiler for SCI Script is part of the impressive [SCICompanion], which provides a large collection of tools to create an SCI game. Despite its popularity, it has a number of limitations that make running public projects with it difficult:

1. It is Windows only.

   The current version of SCICompanion is written in MFC, and even the internals of the compiler depend heavily on the framework. This means using it on other platforms (Linux and MacOS, for instance) is not feasable.

2. It is GUI-only.

   Since it uses MFC in such a fundamental way, it is not designed to compile SCI script files from the command line. This means that headless builds, and most build tools (CMake, GMake, Bazel, etc.) aren't viable to reliably build the compiled files from scratch without manual intervention.

3. It requires the entire game resources to be available to build

   When SCICompanion builds a game, the only output it has are the resource files for the target game. This means in order to build the program you need to have the entire game available as input to the build. While this would be fine with games written from scratch, trying to publicly work on modifications to existing games would require the entire game to be added as part of the project repository which would effectively make the entire game free to download. This would probably make the original rights holders mad, which means a higher likelihoold of cease-and-desist letters.

4. It uses binary files as compiler inputs.

   SCICompiler uses `*.sco` files to keep data needed to compile scripts. While this is not a terrible thing, binary files tend to be much harder to work with within a version control system (such as [Git]), as showing human-readable diffs and merges become significantly harder to work with.

`scinc` attempts to solve all of these problems, enabling easier collaboration for modification projects for classic Sierra games, as well as providing a tool that does one thing well, and can be used by other projects as an self-contained program.

## Getting Started

{: .warning }
> These instructions are not finalized yet. We will likely be creating binary releases of the compiler which can be used directly, as well as providing support for [Bazel] as a build system. Other build systems can be contributed.

Right now, in order to use `scinc`, you need to build it from source. Follow the directions on the Building page (*not yet available*).

[github-repo]: https://github.com/naerbnic/sci-compiler
[ScummVM]: https://www.scummvm.org/
[Bazel]: https://bazel.build/
[Git]: https://git-scm.com/
[SCICompanion]: https://scicompanion.com/Documentation/index.html
