# Sierra Creative Interpreter Script Compiler

The SCI Script Compiler is a command-line tool that compiles SCI game scripts
into resources that can be used to build SCI engine games, or to patch existing
Sierra SCI games. It is intended to be buildable on MacOS and Linux, with possible Windows support in the future.

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
bazel build //src:sc
```

This may take a while, as the default configuration has to download several
tools, including the LLVM compiler. When complete, the command will report
where you can find the binary file.

[Bazel]: https://bazel.build/

## Bugs

We intend to support MacOS, Linux. Windows support has to be tested and
debugged. If you run into any build issues, please report bugs to this
project.
