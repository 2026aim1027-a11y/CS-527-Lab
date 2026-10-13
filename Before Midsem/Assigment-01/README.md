# Mini Computer Simulator

This project is a simple **Mini Computer Simulator** developed in **C** as part of a Computer Organization and Architecture laboratory assignment. It demonstrates the basic working of a processor by simulating the **Fetch → Decode → Execute** cycle.

The simulator first compiles a small assembly-like language into bytecode and then executes the generated instructions using a simulated processor and memory.

## Features

- Converts assembly-like instructions into machine bytecode (`program.byte`)
- Simulates a processor with **256 general-purpose registers**
- Supports basic arithmetic operations:
  - Addition (`+`)
  - Subtraction (`-`)
  - Multiplication (`*`)
  - Division (`/`)
- Reads data from memory and writes results back to memory
- Executes instructions using the **Fetch → Decode → Execute** cycle
- Updates and stores the final memory state in `data.byte`

---

## Project Structure

```text
.
├── main.c
├── compiler.c
├── compiler.h
├── processor.c
├── processor.h
├── memory.c
├── memory.h
├── input.txt
├── data.byte
├── program.byte
└── README.md
```

---

## Supported Instructions

| Instruction | Description |
|------------|-------------|
| `Read xR, A` | Load the value from memory address `A` into register `xR` |
| `Write xR, A` | Store the value from register `xR` into memory address `A` |
| `xD = xS1 + xS2` | Add two registers |
| `xD = xS1 - xS2` | Subtract two registers |
| `xD = xS1 * xS2` | Multiply two registers |
| `xD = xS1 / xS2` | Divide two registers |
| `xD = V` | Load an immediate value into register `xD` |

---

## Building the Project

Compile the project using GCC:

```bash
gcc main.c compiler.c processor.c memory.c -o compiler
```

---

## Running the Simulator

On Windows:

```powershell
.\compiler.exe
```

---

## Example Input

```text
Read x1, 0
Read x2, 4
x3 = x1 + x2
Write x3, 8
```

---

## Output Files

After execution, the following files are generated or updated:

- **program.byte** – Contains the compiled bytecode generated from `input.txt`.
- **data.byte** – Stores the final state of the data memory after program execution.

---

## Author

**Kuber Pathak**  
M.Tech in Artificial Intelligence & Data Engineering  
Indian Institute of Technology Ropar