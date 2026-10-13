#include "compiler.h"
#include "memory.h"
#include "processor.h"


int main() {
    compile();
    initialise();
    reset();
    while(!end_of_simulation) {
        fetch();
        decode();
        execute();
    }
    finalize();
    return 0;
}