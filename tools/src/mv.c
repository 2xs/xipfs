#include "mkxipfs.h"

#include <stdio.h>
#include <string.h>

int cmd_mv(app_ctx_t *ctx, int argc, char **argv) {
    char src[XIPFS_PATH_MAX];
    char dst[XIPFS_PATH_MAX];
    int ret;
    if (argc != 2) {
        fprintf(stderr, "Error: mv expects <xipfs_src> <xipfs_dst>.\n");
        return 1;
    }
    if (make_xipfs_path(argv[0], src, sizeof(src)) < 0) {
        fprintf(stderr, "Error: invalid source path '%s'.\n", argv[0]);
        return 1;
    }
    if (make_xipfs_path(argv[1], dst, sizeof(dst)) < 0) {
        fprintf(stderr, "Error: invalid destination path '%s'.\n", argv[1]);
        return 1;
    }
    ret = xipfs_rename(&ctx->mount, src, dst);
    if (ret < 0) {
        fprintf(stderr, "Error: mv failed '%s' -> '%s' (%s).\n", src, dst, strerror(-ret));
        return 1;
    }
    return 0;
}
