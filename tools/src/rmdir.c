#include "mkxipfs.h"

#include <stdio.h>
#include <string.h>

int cmd_rmdir(app_ctx_t *ctx, int argc, char **argv) {
    char path[XIPFS_PATH_MAX];
    int ret;
    if (argc != 1) {
        fprintf(stderr, "Error: rmdir expects <xipfs_dir>.\n");
        return 1;
    }
    if (make_xipfs_path(argv[0], path, sizeof(path)) < 0) {
        fprintf(stderr, "Error: invalid xipfs path '%s'.\n", argv[0]);
        return 1;
    }
    ret = xipfs_rmdir(&ctx->mount, path);
    if (ret < 0) {
        fprintf(stderr, "Error: rmdir failed for '%s' (%s).\n", path, strerror(-ret));
        return 1;
    }
    return 0;
}
