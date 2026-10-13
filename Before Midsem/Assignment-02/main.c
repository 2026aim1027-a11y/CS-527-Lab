#include <stdio.h>

#include "compiler.h"
#include "processor.h"
#include "memory.h"

int main(int argc, char **argv) {
    const char *source_file    = (argc > 1) ? argv[1] : "program.txt";
    const char *program_byte   = "program.byte";
    const char *data_byte      = "data.byte";

    if (compile(source_file, program_byte) != 0) {
        fprintf(stderr, "main: compilation failed for %s\n", source_file);
        return 1;
    }

    if (initialize(program_byte, data_byte) != 0) {
        fprintf(stderr, "main: failed to initialize memory\n");
        return 1;
    }

    reset();

    while (!end_of_simulation) {
        fetch();
        decode();
        execute();
    }

    if (finalize(data_byte) != 0) {
        fprintf(stderr, "main: failed to write %s\n", data_byte);
        return 1;
    }

    printf("Simulation finished. Final PC=%d\n", PC);
    return 0;
}
