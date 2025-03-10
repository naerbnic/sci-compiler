# Sierra Creative Interpreter Script Compiler

The **SCI Script Compiler** is a command-line tool for compiling SCI game scripts into resources that can be used to build new SCI engine games or patch existing Sierra SCI games.

This project is a **work in progress**, and the interface is subject to change. Contributions and feedback are welcome!

## 🚀 Features

- Compiles SCI scripts into game-compatible resources.
- Intended to support **Windows, Linux, and macOS**.
- Forked from the original [Sierra Creative Interpreter Script Compiler][da-sci] by **Digital Alchemy** and **Daniel Arnold (Dhel)**.
- Actively developed with significant improvements and a refactored codebase.

## 📦 Installation & Compilation

This project uses [Bazel] for building. To get started:

### **1. Clone the Repository**

```sh
git clone https://github.com/YOUR-USERNAME/your-repo.git
cd your-repo
```

### **2. Install Bazel**

Follow the instructions on the [Bazel website](https://bazel.build/) to install Bazel for your system.

### **3. Build the Compiler**

Run the following command:

```sh
bazel build //scic/frontend:scic
```

This will generate the compiler binary in `./bazel-bin/scic/frontend/scic`. You
can call it directly from there.

## 📖 Usage

*(Coming soon: Detailed usage examples and documentation!)*

The basic workflow involves running:

```sh
./bazel-bin/scic/frontend/scic -G <sci_1_1.sh header> -I <include path> -o <output directory> <input_scripts...> 
```

More details will be added as the interface stabilizes.

## 🛠️ Roadmap

We are working on:

- Improving usability and command-line options.
- Improving compatibility with existing SCICompanion code.
- Adding better error handling and diagnostics.
- Documentation and example projects.

## 🐞 Reporting Issues & Contributing

We intend to support **Windows, macOS, and Linux**. If you encounter build issues, crashes, or unexpected behavior, please [open an issue](https://github.com/naerbnic/sci-compiler/issues).

### **Want to Contribute?**

- Check out the open [issues](https://github.com/naerbnic/sci-compiler/issues) and [project roadmap](https://github.com/naerbnic/sci-compiler/projects).
- Fork the repo and submit a pull request!

## 📜 License

This project is licensed under the MIT license. See [the license file](https://github.com/naerbnic/sci-compiler/blob/main/LICENSE.md) for details.

---

[da-sci]: https://github.com/Digital-Alchemy-Studios/da-sci-compiler-pub
[Bazel]: https://bazel.build/
