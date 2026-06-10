# CS-471L Mini Compiler

A fully integrated compiler for a custom Pascal-like language, featuring a Lexical Analyzer, Semantic Analyzer, Recursive Descent Parser (AST builder), Predictive LL(1) Parser, and a Canonical LR(1) Parser.

The project also includes a rich, web-based UI Dashboard to visualize the pipeline, traces, abstract syntax trees, and error handling.

## Requirements

### Backend / C++ Compiler
- `g++` (Compiler with C++17 support)
- `make` (for building the project)
- Linux / WSL (Recommended for standard building)

### Frontend / Dashboard
- `python3` (for running the local web server)

---

## 1. Building the Compiler

To build the compiler executable, open your terminal (e.g., WSL or Linux shell), navigate to this project folder, and run:

```bash
make
```

This will compile the `src/*.cpp` files and generate an executable named `mini_compiler`.

To clean the compiled binaries:
```bash
make clean
```

---

## 2. Running the Compiler (Terminal / Command Line)

Once compiled, you can run the compiler against any Pascal source file.

**Basic Run:**
```bash
./mini_compiler test/valid_nested.pascal
```

**Run Multiple Files:**
```bash
./mini_compiler test/valid_nested.pascal test/duplicate_same_scope.pascal
```

**Run with LR Parser trace enabled:**
The LR parser trace can be very long. To enable it alongside the default parsers, use the `--with-lr` flag:
```bash
./mini_compiler --with-lr test/valid_nested.pascal
```

**Generate Documentation (Grammar, Tables, etc.):**
```bash
./mini_compiler --dump-docs
```
This will generate `first_follow.txt`, `ll1_table.txt`, `lr_table.txt`, and `grammar_bnf.txt` in the `docs/` folder.

---

## 3. Running the Dashboard (Web UI)

The project comes with a beautiful web-based interface that allows you to write custom code or load test files, and view the entire compiler pipeline trace, visual AST trees, and semantic symbol table dumps.

To launch the dashboard:

1. Open a terminal in the project root directory.
2. Navigate to the `ui/` directory:
   ```bash
   cd ui
   ```
3. Run the Python server:
   ```bash
   python3 server.py
   ```
4. Open your web browser and go to:
   **http://localhost:8000**

From the dashboard, you can click **"Run sample"** or write code in the editor and click **"Run custom"**. Explore the **Trace**, **AST Tree**, **Final Results**, and **Docs** tabs!
