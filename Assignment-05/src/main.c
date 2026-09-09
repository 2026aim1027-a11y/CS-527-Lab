#include "os.h"

/* Lab 4 moves compile()/initialise()/reset() and the fetch-decode-execute
 * loop out of main() and into the OS layer, exactly as the handout's
 * comments on the old main() pseudocode say ("This will also move to
 * OS..."): main() now just starts the OS, which loads tasks (via its
 * shell/loader) and multiplexes them across NUM_PROCESSORS processors
 * (via its scheduler) until told to stop.
 *
 * Usage: ./sim [initial_program[.txt|.byte] [initial_data.byte]]
 *   The optional arguments pre-load one task at startup, in addition to
 *   (not instead of) the interactive "$ " shell prompt for loading more.
 */
int main(int argc, char **argv) {
    os_init(argc, argv);
    os_run();
    return 0;
}
