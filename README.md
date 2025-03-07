# Sierra Creative Interpreter Script Compiler

The SCI Script Compiler is a command-line tool that compiles SCI game scripts
into resources that can be used to build SCI engine games, or to patch existing
Sierra SCI games. We intend to support Windows, Linux, and MacOS.

This is a **work in progress**, and the interface to the tool is in flux at
this moment.

This is a fork of the original
[Sierra Creative Interpreter Script Compiler][da-sci] by Digital Alchemy and
Daniel Arnold (Dhel). There have been sufficient changes in the codebase that
I believe a full detached fork is warranted.

[da-sci]: https://github.com/Digital-Alchemy-Studios/da-sci-compiler-pub

## Compiling

The main build requires the [Bazel] build tool. To build, check out this
repository, install Bazel, then run the following command:

```shell
bazel build //scic/frontend:scic
```

This will build the compiler in `bazel-bin/scic/frontend/scic`, which can be used directly.

[Bazel]: https://bazel.build/

## Bugs

We intend to support Windows, MacOS, and Linux. If you run into any build issues, please report bugs to this
project.
