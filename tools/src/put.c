#include "mkxipfs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int cmd_put(app_ctx_t *ctx, int argc, char **argv) {
    const char *target_raw;
    char target[XIPFS_PATH_MAX];
    unsigned char *data = NULL;
    size_t size = 0;
    uint32_t exec = 0;
    int ret = 0;
    if (argc == 2) {
        const char *host_file = argv[0];
        bool is_fae = false;
        int fd = open(host_file, O_RDONLY);
        if (fd < 0) {
            perror("open host file");
            return 1;
        }
        if (host_file_is_fae(host_file, &is_fae) == 0 && is_fae)
            exec = 1;
        if (read_all_fd(fd, &data, &size) < 0) {
            (void)close(fd);
            return 1;
        }
        (void)close(fd);
        target_raw = argv[1];
    }
    else if (argc == 1) {
        if (read_all_fd(STDIN_FILENO, &data, &size) < 0)
            return 1;
        if (is_fae_payload(data, size))
            exec = 1;
        target_raw = argv[0];
    }
    else {
        fprintf(stderr, "Error: put expects <host_file> <xipfs_file> or <xipfs_file>.\n");
        return 1;
    }
    if (size > UINT32_MAX) {
        fprintf(stderr, "Error: input too large for xipfs (%zu bytes).\n", size);
        free(data);
        return 1;
    }
    if (make_xipfs_path(target_raw, target, sizeof(target)) < 0) {
        fprintf(stderr, "Error: invalid xipfs path '%s'.\n", target_raw);
        free(data);
        return 1;
    }
    (void)xipfs_unlink(&ctx->mount, target);
    int new_ret = xipfs_new_file(&ctx->mount, target, (uint32_t)size, exec);
    if (new_ret < 0) {
        fprintf(stderr, "Error: xipfs_new_file failed for '%s' (%s).\n", target, strerror(-new_ret));
        free(data);
        return 1;
    }
    xipfs_file_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    if (xipfs_open(&ctx->mount, &desc, target, O_WRONLY, 0) < 0) {
        fprintf(stderr, "Error: cannot open '%s' for write.\n", target);
        free(data);
        return 1;
    }
    size_t off = 0;
    while (off < size) {
        ssize_t written = xipfs_write(&ctx->mount, &desc, data + off, size - off);
        if (written < 0) {
            fprintf(stderr, "Error: write failed on '%s'.\n", target);
            ret=1;
            break;
        }
        if (written == 0 && (size - off) > 0) {
            fprintf(stderr, "Error: no space left while writing '%s'.\n", target);
            ret=1;
            break;
        }
        off += (size_t)written;
    }
    if (xipfs_close(&ctx->mount, &desc) < 0)
        ret = 1;
    free(data);
    return ret;
}
