---
title: Development
layout: default
nav_order: 2
description: "Documentation on developing scinc"
---

# Developer Documentation

These documentation pages give information needed to change, build, and test `scinc`.

## Prerequisites

Some tools must be available before you can build the compiler.

### C++ Compiler

We support building on Windows, Linux, and MacOS.

#### Windows

A Visual Studio C++ environment that supports C++20 must be available. You can install [Visual Studio 2022] Community Edititon for free, which will be sufficient.

Later instructions will ask you to run commands on the command line. You should do this within the [Visual Studio Developer Command Prompt][VSDevPrompt], as that will ensure that all of the configuration for your C++ compiler should be available.

#### Linux

Either [Clang] or [GCC] must be available. There are many flavors of Linux, so we can't provide all the ways to obtain the compiler, but your distribution's package manager should have a package that provides the necessary environment.

Commands can be executed in any terminal. Commands should be shell independent.

#### MacOS

The system standard version of Clang must be available. This can be installed by installing [Xcode] for your system. You must also install the command-line tools, which you can do through the following command:

```shell
$ xcode-select --install
```

This will open up a dialogue to install the command line tools.

### Bazel

We use the [Bazel] build tool within the `scinc` repository. Following [these instructions][BazelInstall] should make the tool available on your system. You should check if bazel is available by opening up an appropriate command line, and running:

```shell
$ bazel --version
bazel 8.1.1
```

Your version number may vary.

### Git

You need to have [Git] available in order to work with the code. You can get this a number of ways:

- [Install directly for your platform][GitInstall]
- [Install it as part of the GitHub CLI][GitHubCli]

## Downloading The Code

You can download the code from our [GitHub Repo][ScincRepo]. If you intend to help develop, you should [fork the repo][GitHubFork] to your own account, but you can clone the repository directly from the current project.

Using git, you can clone this into your preferred directory:

```shell
$ git clone https://github.com/naerbnic/sci-compiler.git
Cloning into 'sci-compiler'...
remote: Enumerating objects: 5371, done.
remote: Counting objects: 100% (774/774), done.
remote: Compressing objects: 100% (122/122), done.
remote: Total 5371 (delta 672), reused 695 (delta 644), pack-reused 4597 (from 1)
Receiving objects: 100% (5371/5371), 1.16 MiB | 5.25 MiB/s, done.
Resolving deltas: 100% (4134/4134), done.
$ cd sci-compiler
```

## Building `scinc`

You can build `scinc` now by building via Bazel:

```shell
$ bazel build //scic/frontend:scic
Starting local Bazel server (8.1.1) and connecting to it...
...
INFO: Found 1 target...
Target //scic/frontend:scic up-to-date:
  bazel-bin/scic/frontend/scic
INFO: Elapsed time: 17.679s, Critical Path: 2.81s
INFO: 354 processes: 136 internal, 218 darwin-sandbox.
INFO: Build completed successfully, 354 total actions
```

The binary should now be available at `bazel-bin/scic/frontend/scic`

[Clang]: https://clang.llvm.org/
[GCC]: https://gcc.gnu.org/
[Git]: https://git-scm.com/
[Visual Studio 2022]: https://visualstudio.microsoft.com/vs/
[VSDevPrompt]: https://learn.microsoft.com/en-us/visualstudio/ide/reference/command-prompt-powershell?view=vs-2022
[Xcode]: https://developer.apple.com/xcode/
[BazelInstall]: https://bazel.build/install
[ScincRepo]: https://github.com/naerbnic/sci-compiler
[GitInstall]: https://git-scm.com/downloads
[GitHubCli]: https://cli.github.com/
[GitHubFork]: https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/working-with-forks/fork-a-repo
