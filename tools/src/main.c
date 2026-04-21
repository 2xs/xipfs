#include "mkxipfs.h"
#include "include/buffer.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool command_requires_rw(const char *command)
{
    return (strcmp(command, "put") == 0) ||
           (strcmp(command, "mkdir") == 0) ||
           (strcmp(command, "rm") == 0) ||
           (strcmp(command, "rmdir") == 0) ||
           (strcmp(command, "mv") == 0);
}

int main(int argc, char **argv)
{
    int i = 1;
    const char *flash_opt = NULL;
    const char *target_opt = NULL;
    const char *command;

    while (i < argc && strncmp(argv[i], "--", 2) == 0) {
        if (strcmp(argv[i], "--flash") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --flash expects a filename.\n");
                usage(argv[0]);
                return 1;
            }
            flash_opt = argv[i + 1];
            i += 2;
            continue;
        }
        if (strcmp(argv[i], "--target") == 0) {
            if (i +1 >= argc) {
                fprintf(stderr, "Error: --target expects a target name.\n");
                usage(argv[0]);
                return 1;
            }
            target_opt = argv[i + 1];
            i+=2;
            continue;
        }
        if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
        fprintf(stderr, "Error: unknown option '%s'.\n", argv[i]);
        usage(argv[0]);
        return 1;
    }

    if (i >= argc) {
        usage(argv[0]);
        return 1;
    }

    const char *target = resolve_target(target_opt);
    if (target == NULL) {
        fprintf(stderr,
                "Error: no target selected.\n"
                "Use --target <target_name> or enter \"export XIPFS_TARGET=<target_name>\".\n");
        return 1;
    }

    if (set_target_nvm_configuration(target) != 0) {
        fprintf(stderr,
                "Error: Failed to set target \"%s\".\n", target);
        return 1;
    }
    print_current_target_configuration();

    if (xipfs_buffer_allocate() < 0) {
        fprintf(stderr,
                "Error: Failed to allocate buffer\n");
        return 1;
    }

    command = argv[i++];

    if (strcmp(command, "create") == 0) {
        int rc = cmd_create(argc - i, &argv[i]);
        xipfs_buffer_free();
        return rc;
    }
    if (strcmp(command, "build") == 0) {
        int rc = cmd_build(argc - i, &argv[i]);
        xipfs_buffer_free();
        return rc;
    }
    if (strcmp(command, "test") == 0) {
        if ((argc - i) != 0) {
            fprintf(stderr, "Error: test does not accept extra arguments.\n");
            usage(argv[0]);
            xipfs_buffer_free();
            return 1;
        }
        int rc = cmd_test();
        xipfs_buffer_free();
        return rc;
    }
    if (strcmp(command, "test_deep") == 0) {
        if ((argc - i) != 0) {
            fprintf(stderr, "Error: test_deep does not accept extra arguments.\n");
            usage(argv[0]);
            xipfs_buffer_free();
            return 1;
        }
        int rc = cmd_test_deep();
        xipfs_buffer_free();
        return rc;
    }

    const char *flash = resolve_flash_image(flash_opt);
    if (flash == NULL) {
        fprintf(stderr,
                "Error: no flash image selected.\n"
                "Use --flash <filename.flash> or export XIPFS_FILE_IMAGE.\n");
        xipfs_buffer_free();
        return 1;
    }

    app_ctx_t ctx;
    if (open_image(&ctx, flash, command_requires_rw(command)) < 0) {
        xipfs_buffer_free();
        return 1;
    }

    int rc;
    if (strcmp(command, "ls") == 0) {
        rc = cmd_ls(&ctx, argc - i, &argv[i]);
    }
    else if (strcmp(command, "tree") == 0) {
        rc = cmd_tree(&ctx, argc - i, &argv[i]);
    }
    else if (strcmp(command, "put") == 0) {
        rc = cmd_put(&ctx, argc - i, &argv[i]);
    }
    else if (strcmp(command, "get") == 0) {
        rc = cmd_get(&ctx, argc - i, &argv[i]);
    }
    else if (strcmp(command, "mkdir") == 0) {
        rc = cmd_mkdir(&ctx, argc - i, &argv[i]);
    }
    else if (strcmp(command, "rm") == 0) {
        rc = cmd_rm(&ctx, argc - i, &argv[i]);
    }
    else if (strcmp(command, "rmdir") == 0) {
        rc = cmd_rmdir(&ctx, argc - i, &argv[i]);
    }
    else if (strcmp(command, "mv") == 0) {
        rc = cmd_mv(&ctx, argc - i, &argv[i]);
    }
    else {
        fprintf(stderr, "Error: unknown command '%s'.\n", command);
        usage(argv[0]);
        rc = 1;
    }

    close_image(&ctx);
    xipfs_buffer_free();
    return rc;
}
