#include <stdio.h>
#include <stdlib.h>
#include "compiler.h"
#include "memory.h"
#include "processor.h"

#define MAX_STEPS 1000000  /* safety net against runaway/infinite loops */

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s <program.txt | program.byte> [data.byte] [output_data.byte]\n\n"
        "  program.txt / program.byte  Either an assembly source file or an\n"
        "                              already-compiled program.byte. Either\n"
        "                              way, the compiled bytecode is written\n"
        "                              to 'program.byte' in the current directory.\n"
        "  data.byte                   Optional initial data memory image.\n"
        "  output_data.byte            Optional path to write the final data\n"
        "                              memory to (defaults to overwriting\n"
        "                              data.byte, or './data.byte' if no\n"
        "                              data.byte was given).\n",
        prog);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    const char *src        = argv[1];
    const char *data_in     = (argc >= 3) ? argv[2] : NULL;
    const char *data_out    = (argc >= 4) ? argv[3] : (data_in ? data_in : "data.byte");
    const char *program_byte = "program.byte";

    compile(src, program_byte);
    mem_initialize(program_byte, data_in);
    reset();

    long steps = 0;
    while (!end_of_simulation) {
        fetch();
        decode();
        execute();
        if (++steps > MAX_STEPS) {
            fprintf(stderr, "Error: exceeded %d instructions without halting - possible infinite loop\n", MAX_STEPS);
            break;
        }
    }

    mem_finalize(data_out);

    fprintf(stderr, "Simulation finished after %ld instruction(s).\n", steps);
    return 0;
}
