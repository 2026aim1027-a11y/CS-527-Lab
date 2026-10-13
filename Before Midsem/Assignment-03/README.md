# CS527 Lab 3 - Vector-Extended Mini-Computer Simulator

## Build

```
make
```

Produces an executable `./sim`.

## Run

```
./sim <source.txt | program.byte> [data.byte] [output_data.byte]
```

- The first argument may be either an assembly source file (as defined
  by the language spec) or an already-compiled `program.byte` - the
  compiler auto-detects which one it was given (see "Compile vs. run"
  below). Either way, the compiled bytecode is (re)written to
  `program.byte` in the current directory, satisfying the "compilation
  and computer system invoked by a single main file / single
  executable" requirement.
- `data.byte`, if given, is loaded into the 4096-byte data memory
  before execution.
- `output_data.byte`, if given, is where the final data memory is
  written; otherwise it defaults to overwriting `data.byte` (or to
  `./data.byte` if no input data file was given).

Example:

```
make
./sim tests/array_add.txt tests/array_add_data.byte out.byte
```

## Compile vs. run

`main.c`'s flow is exactly the pseudocode given in the assignment:
`compile(); initialize(); reset(); while(!end_of_simulation) { fetch();
decode(); execute(); } finalize();`. `compile()` looks at the input
file: if every line is already exactly 4 space-separated hex bytes, it
is treated as a pre-compiled `program.byte` and copied through
unchanged; otherwise it's parsed as assembly. This lets the same
executable satisfy both "compiler generates program.byte from source"
and "executable takes program.byte and data.byte as command line
input".

## Directory layout

```
src/main.c        - drives compile -> initialize -> reset -> fetch/decode/execute loop -> finalize
src/compiler.c/.h - two-pass assembler: source text -> program.byte
src/processor.c/.h- Register[256], VecRegister[32][8], PC, flags, fetch/decode/execute
src/memory.c/.h   - Instruction[256]/Data[4096], hex-file load/save, word read/write
src/opcodes.h     - the bytecode opcode map, shared by compiler and processor
Makefile
tests/            - sample programs + generated test data, for our own verification
```

## Spec issues found, and how they were resolved

The assignment handout has two internal inconsistencies. Both were
resolved by hand and are called out here (and in comments at the top
of `opcodes.h`) rather than silently patched over:

1. **Opcode collision.** The table lists both "Divide (constant
   operand)" and "Integer Memory read (constant address)" as `0x0C`.
   Divide keeps `0x0C`; Integer-Memory-read-with-constant-address was
   moved to the unused `0x0D`.

2. **Vector second-operand ambiguity.** The language spec shows three
   distinct source forms for vector arithmetic - `v3 = v1 + v4`
   (vector operand), `v3 = v1 * x4` (scalar register operand), and
   `v3 = v1 + 15` (constant operand) - but the opcode table only has
   two columns (`var` / `const`) for vector add/sub/multiply. Since a
   vector-register index and an integer-register index are both just
   plain 0-31 / 0-255 byte fields with nothing else in the encoding to
   tell them apart, "var" can't mean both at once. A third opcode band
   (`0x31-0x33`) was added for the vector-op-scalar-register form;
   `0x21-0x23` is vector-op-vector and `0x29-0x2B` is vector-op-constant.

Everything else in the opcode map was cross-checked against the one
fully worked example in the handout (the array-sum program and its
byte-for-byte bytecode dump) - `./sim tests/handout_example.txt`
reproduces that exact dump. That worked example is also what pins down
two things the prose alone leaves ambiguous:

- **Branch offsets are encoded in instruction-count units, not bytes**
  (`offset = target_instruction_index - branch_instruction_index`), even
  though the prose describes it as "current instruction address +
  offset". The processor multiplies by 4 when it applies the offset to PC.
- **Register-field conventions for memory ops**: for a read, `dest` is
  the destination register and `src2` is the address (register or
  constant); for a write, `dest` is the address (register or constant)
  and `src2` is the value register. (`src1` is always 0, exactly as the
  spec states.)

## Other design choices worth knowing about

- **No vector reduction instruction exists** in the ISA (only
  elementwise vector ops and whole-vector load/store), so the FIR
  filter's 8-tap dot product is computed by vector-multiplying the
  input window by the weight vector, spilling that 8-wide product to a
  scratch address in data memory, then summing those 8 words back up
  with ordinary scalar adds. See the comment block at the top of
  `tests/fir.txt`.
- **Register-to-register copies** (`x1 = x2`, `v1 = v2`) aren't a
  separate opcode - the language only defines data movement as "dest =
  constant". A copy is compiled as `dest = src + 0`.
- **Legacy `Read`/`Write`** are supported and compile to the constant-
  address memory read/write opcodes.
- **Constants are enforced to the spec's 0-255 range** at compile time
  (an out-of-range immediate is a compile error, not silently
  truncated). Addresses bigger than 255 have to be built at runtime
  with arithmetic - see `tests/fir.txt` for an example.
- **Flags (Z N C V)** are only updated by scalar integer add/sub, per
  the spec's wording ("every addition/subtraction operation..." in the
  context of the x-registers); vector add/sub and all multiply/divide
  leave flags unchanged.
- The compiler automatically appends a halt (`0 0 0 0`) instruction if
  the source doesn't end with one, since opcode `0` is what the
  processor's `execute()` uses to set `end_of_simulation`.
- Data memory reads/writes are little-endian 32-bit words (the spec's
  only worked example, `-1 = FF FF FF FF`, doesn't distinguish byte
  order, so this is a documented assumption, not a derived fact).

## Test programs

- `tests/handout_example.txt` - the array-sum program from the
  assignment's own worked example; used to confirm the compiler's
  bytecode output matches the handout's dump exactly.
- `tests/array_add.txt` - vector array-add (rubric item: "Array add
  program").
- `tests/fir.txt` - vector FIR filter (rubric item: "FIR filter
  program").

All three were run end-to-end (compiled, executed against generated
`data.byte` inputs, and checked against independently computed
expected results in Python) before this was packaged up.
