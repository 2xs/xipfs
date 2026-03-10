#include "mkxipfs.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int cmd_get(app_ctx_t *ctx, int argc, char **argv) {
    const char *source_raw;
    char source[XIPFS_PATH_MAX];
    int out_fd = STDOUT_FILENO;
    xipfs_file_desc_t desc;
    unsigned char buf[4096];
    if (argc == 2) {
        source_raw = argv[0];
        out_fd = open(argv[1], O_CREAT | O_TRUNC | O_WRONLY, 0644);
        if (out_fd < 0) {
            perror("open output file");
            return 1;
        }
    } else if (argc == 1) {
        source_raw = argv[0];
    } else {
        fprintf(stderr, "Error: get expects <xipfs_file> <host_file> or <xipfs_file>.\n");
        return 1;
    }
    if (make_xipfs_path(source_raw, source, sizeof(source)) < 0) {
        fprintf(stderr, "Error: invalid xipfs path '%s'.\n", source_raw);
        if (out_fd != STDOUT_FILENO) {
            (void)close(out_fd);
        }
        return 1;
    }
    memset(&desc, 0, sizeof(desc));
    if (xipfs_open(&ctx->mount, &desc, source, O_RDONLY, 0) < 0) {
        fprintf(stderr, "Error: cannot open '%s' for read.\n", source);
        if (out_fd != STDOUT_FILENO)
            (void)close(out_fd);
        return 1;
    }
    while (1) {
        ssize_t n = xipfs_read(&ctx->mount, &desc, buf, sizeof(buf));
        if (n < 0) {
            fprintf(stderr, "Error: read failed on '%s'.\n", source);
            (void)xipfs_close(&ctx->mount, &desc);
            if (out_fd != STDOUT_FILENO) {
                (void)close(out_fd);
            }
            return 1;
        }
        if (n == 0) break;
        if (write_all_fd(out_fd, buf, (size_t)n) < 0) {
            (void)xipfs_close(&ctx->mount, &desc);
            if (out_fd != STDOUT_FILENO) (void)close(out_fd);
            return 1;
        }
    }
    if (xipfs_close(&ctx->mount, &desc) < 0) {
        if (out_fd != STDOUT_FILENO) (void)close(out_fd);
        return 1;
    }
    if (out_fd != STDOUT_FILENO && close(out_fd) < 0) {
        perror("close output file");
        return 1;
    }
    return 0;
}
