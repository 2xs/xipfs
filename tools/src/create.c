#include "mkxipfs.h"

#include <stdio.h>

int cmd_create(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Error: create requires <filename.flash> <size>.\n");
        return 1;
    }
    return create_image(argv[0], argv[1]);
}
