#include "os.h"

int main(int argc, char **argv) {
    os_init(argc, argv);
    os_run();
    return 0;
}
