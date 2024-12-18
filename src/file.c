/*******************************************************************************/
/*  © Université de Lille, The Pip Development Team (2015-2024)                */
/*  Copyright (C) 2020-2024 Orange                                             */
/*                                                                             */
/*  This software is a computer program whose purpose is to run a minimal,     */
/*  hypervisor relying on proven properties such as memory isolation.          */
/*                                                                             */
/*  This software is governed by the CeCILL license under French law and       */
/*  abiding by the rules of distribution of free software.  You can  use,      */
/*  modify and/ or redistribute the software under the terms of the CeCILL     */
/*  license as circulated by CEA, CNRS and INRIA at the following URL          */
/*  "http://www.cecill.info".                                                  */
/*                                                                             */
/*  As a counterpart to the access to the source code and  rights to copy,     */
/*  modify and redistribute granted by the license, users are provided only    */
/*  with a limited warranty  and the software's author,  the holder of the     */
/*  economic rights,  and the successive licensors  have only  limited         */
/*  liability.                                                                 */
/*                                                                             */
/*  In this respect, the user's attention is drawn to the risks associated     */
/*  with loading,  using,  modifying and/or developing or reproducing the      */
/*  software by the user in light of its specific status of free software,     */
/*  that may mean  that it is complicated to manipulate,  and  that  also      */
/*  therefore means  that it is reserved for developers  and  experienced      */
/*  professionals having in-depth computer knowledge. Users are therefore      */
/*  encouraged to load and test the software's suitability as regards their    */
/*  requirements in conditions enabling the security of their systems and/or   */
/*  data to be ensured and,  more generally, to use and operate it in the      */
/*  same conditions as regards security.                                       */
/*                                                                             */
/*  The fact that you are presently reading this means that you have had       */
/*  knowledge of the CeCILL license and that you accept its terms.             */
/*******************************************************************************/

/*
 * libc includes
 */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/*
 * xipfs includes
 */
#include "include/xipfs.h"
#include "include/buffer.h"
#include "include/errno.h"
#include "include/file.h"
#include "include/flash.h"

/*
 * Macro definitions
 */

/**
 * @internal
 *
 * @def XIPFS_FREE_RAM_SIZE
 *
 * @brief Amount of free RAM available for the relocatable
 * binary to use
 */
#define XIPFS_FREE_RAM_SIZE (512)

/**
 * @internal
 *
 * @def EXEC_STACKSIZE_DEFAULT
 *
 * @brief The default execution stack size of the binary
 */
#define EXEC_STACKSIZE_DEFAULT 1024

#ifdef __GNUC__
/**
 * @internal
 *
 * @def NAKED
 *
 * @brief Indicates that the specified function does not need
 * prologue/epilogue sequences generated by the compiler
 */
#define NAKED __attribute__((naked))
/**
 * @internal
 *
 * @def UNUSED
 *
 * @brief Indicates that the specified variable or function may
 * be intentionally unused
 */
#define UNUSED __attribute__((unused))
/**
 * @internal
 *
 * @def USED
 *
 * @brief Indicates that the specified static variable or
 * function is to be retained in the object file, even if it is
 * unreferenced
 */
#define USED __attribute__((used))
#else
#error "sys/fs/file: Your compiler does not support GNU extensions"
#endif /* __GNUC__ */

/*
 * Internal structure
 */

/**
 * @internal
 *
 * @warning The order of the members in the enumeration must
 * remain synchronized with the order of the members of the same
 * enumeration declared in file
 * examples/xipfs/hello-world/stdriot/stdriot.c
 *
 * @brief An enumeration describing the index of the functions
 * of the libc and RIOT in the system call table
 */
enum syscall_index_e {
    /**
     * Index of exit(3)
     */
    SYSCALL_EXIT,
    /**
     * Index of printf(3)
     */
    SYSCALL_PRINTF,
    /**
     * Maximum size of the syscall table used by the relocatable
     * binary. It must remain the final element of the
     * enumeration
     */
    SYSCALL_MAX,
};

/**
 * @internal
 *
 * @brief Data structure that describes the memory layout
 * required by the CRT0 to execute the relocatable binary
 */
typedef struct crt0_ctx_s {
    /**
     * Start address of the binary in the NVM
     */
    void *bin_base;
    /**
     * Start address of the available free RAM
     */
    void *ram_start;
    /**
     * End address of the available free RAM
     */
    void *ram_end;
    /**
     * Start address of the free NVM
     */
    void *nvm_start;
    /**
     * End address of the free NVM
     */
    void *nvm_end;
} crt0_ctx_t;

/**
 * @internal
 *
 * @warning If a member of this data structure is added, removed,
 * or moved, the OFFSET variable in the script
 * example/xipfs/hello-world/scripts/gdbinit.py needs to be
 * updated accordingly
 *
 * @brief Data structure that describes the execution context of
 * a relocatable binary
 */
typedef struct exec_ctx_s {
    /**
     * Data structure required by the CRT0 to execute the
     * relocatable binary
     */
    crt0_ctx_t crt0_ctx;
    /**
     * Reserved memory space in RAM for the stack to be used by
     * the relocatable binary
     */
    char stkbot[EXEC_STACKSIZE_DEFAULT-4];
    /**
     * Last word of the stack indicating the top of the stack
     */
    char stktop[4];
    /**
     * Number of arguments passed to the relocatable binary
     */
    int argc;
    /**
     * Arguments passed to the relocatable binary
     */
    char *argv[XIPFS_EXEC_ARGC_MAX];
    /**
     * Table of function pointers for the libc and RIOT
     * functions used by the relocatable binary
     */
    void *syscall_table[SYSCALL_MAX];
    /**
     * Reserved memory space in RAM for the free RAM to be used
     * by the relocatable binary
     */
    char ram_start[XIPFS_FREE_RAM_SIZE-1];
    /**
     * Last byte of the free RAM
     */
    char ram_end;
} exec_ctx_t;

/*
 * Global variables
 */

/**
 * @internal
 *
 * @brief A reference to the stack's state prior to invoking
 * execv(2)
 */
static void *_exec_curr_stack USED;

/**
 * @brief A pointer to a virtual file name
 */
char *xipfs_infos_file = ".xipfs_infos";

/*
 * Helper functions
 */

/**
 * @internal
 *
 * @brief Local implementation of exit(3) passed to the binary
 * through the syscall table
 *
 * @param status The exit(3) status of the binary is stored in
 * the R0 register
 */
/* TODO: Move this function into board-specific functions */
static void NAKED
xipfs_exec_exit(int status UNUSED)
{
    __asm__ volatile
    (
        "   ldr   r4, =_exec_curr_stack   \n"
        "   ldr   sp, [r4]                \n"
        "   pop   {r4, pc}                \n"
    );
}

/**
 * @internal
 *
 * @note This function has the same prototype as the _exec_exit
 * function
 *
 * @brief Starts the execution of the binary in the current RIOT
 * thread
 */
/* TODO: Move this function into board-specific functions */
static void NAKED
xipfs_exec_enter(crt0_ctx_t *crt0_ctx UNUSED,
                 void *entry_point UNUSED,
                 void *stack_top UNUSED)
{
    __asm__ volatile
    (
        "   push   {r4, lr}               \n"
        "   ldr    r4, =_exec_curr_stack  \n"
        "   str    sp, [r4]               \n"
        "   ldr    r0, =exec_ctx          \n"
        "   add    r4, r0, #1040          \n"
        "   mov    sp, r4                 \n"
        "   ldr    r4, =_exec_entry_point \n"
        "   ldr    r4, [r4]               \n"
        "   blx    r4                     \n"
    );
}

/**
 * @internal
 *
 * @brief
 */
static inline void
exec_ctx_cleanup(exec_ctx_t *exec_ctx)
{
    (void)memset(&exec_ctx, 0, sizeof(*exec_ctx));
}

/**
 * @internal
 *
 * @brief Fills the CRT0 data structure
 *
 * @param ctx A pointer to a memory region containing an
 * accessible execution context
 *
 * @param filp A pointer to a memory region containing an
 * accessible xipfs file structure
 */
static inline void
exec_ctx_crt0_init(exec_ctx_t *ctx, xipfs_file_t *filp)
{
    crt0_ctx_t *crt0_ctx;
    size_t size;
    void *end;

    crt0_ctx = &ctx->crt0_ctx;
    crt0_ctx->bin_base = filp->buf;
    crt0_ctx->ram_start = ctx->ram_start;
    crt0_ctx->ram_end = &ctx->ram_end;
    size = xipfs_file_get_size_(filp);
    crt0_ctx->nvm_start = &filp->buf[size];
    end = (char *)filp + filp->reserved;
    crt0_ctx->nvm_end = end;
}

/**
 * @internal
 *
 * @brief Copies argument pointers to the execution context
 *
 * @param ctx A pointer to a memory region containing an
 * accessible execution context
 *
 * @param argv A pointer to a list of pointers to memory regions
 * containing accessible arguments to pass to the binary
 */
static inline void
exec_ctx_args_init(exec_ctx_t *ctx, char *const argv[])
{
    while (ctx->argc < XIPFS_EXEC_ARGC_MAX && argv[ctx->argc] != NULL) {
        ctx->argv[ctx->argc] = argv[ctx->argc];
        ctx->argc++;
    }
}

/**
 * @internal
 *
 * @brief Fills the syscall table with libc and RIOT function
 * addresses
 *
 * @param ctx A pointer to a memory region containing an
 * accessible execution context
 */
static inline void
exec_ctx_syscall_init(exec_ctx_t *ctx)
{
    ctx->syscall_table[SYSCALL_EXIT] = xipfs_exec_exit;
    ctx->syscall_table[SYSCALL_PRINTF] = vprintf;
}

/**
 * @internal
 *
 * @brief
 */
static inline void
exec_ctx_init(exec_ctx_t *exec_ctx, xipfs_file_t *filp,
              char *const argv[])
{
    exec_ctx_crt0_init(exec_ctx, filp);
    exec_ctx_args_init(exec_ctx, argv);
    exec_ctx_syscall_init(exec_ctx);
}

/*
 * Extern functions
 */

/**
 * @internal
 *
 * @brief Checks if the character passed as an argument is in
 * the xipfs charset
 *
 * @param c The character to check
 *
 * @return Returns one if the character passed as an argument is
 * in the xipfs charset or a zero otherwise
 */
static int
xipfs_file_path_charset_check(char c)
{
    return (c >= '0' && c <= '9') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
            c == '/' || c == '.'  ||
            c == '-' || c == '_';
}

/**
 * @pre path must be a pointer that references a path which is
 * accessible, null-terminated, starts with a slash, normalized,
 * and shorter than XIPFS_PATH_MAX
 *
 * @brief Checks if the path passed as an argument is a valid
 * xipfs path
 *
 * @param path The path to check
 *
 * @return Returns zero if the path passed an an argument is a
 * valid xipfs path or a negative value otherwise
 */
int
xipfs_file_path_check(const char *path)
{
    size_t i;

    if (path == NULL) {
        xipfs_errno = XIPFS_ENULLP;
        return -1;
    }
    if (path[0] == '\0') {
        xipfs_errno = XIPFS_EEMPTY;
        return -1;
    }
    for (i = 0; i < XIPFS_PATH_MAX && path[i] != '\0'; i++) {
        if (xipfs_file_path_charset_check(path[i]) == 0) {
            xipfs_errno = XIPFS_EINVAL;
            return -1;
        }
    }
    if (path[i] != '\0') {
        xipfs_errno = XIPFS_ENULTER;
        return -1;
    }

    return 0;
}

/**
 * @pre filp must be a pointer to an accessible and valid xipfs
 * file structure
 *
 * @brief Checks if the xipfs file structure passed as an
 * argument is a valid one
 *
 * @param filp The xipfs file structure to check
 *
 * @return Returns zero if the xipfs file structure passed as an
 * argument is a valid one or a negative value otherwise
 */
int
xipfs_file_filp_check(xipfs_file_t *filp)
{
    if (filp == NULL) {
        xipfs_errno = XIPFS_ENULLF;
        return -1;
    }
    if (xipfs_flash_page_aligned(filp) < 0) {
        xipfs_errno = XIPFS_EALIGN;
        return -1;
    }
    if (xipfs_flash_in(filp) < 0) {
        xipfs_errno = XIPFS_EOUTNVM;
        return -1;
    }
    if (filp->next == NULL) {
        xipfs_errno = XIPFS_ENULLF;
        return -1;
    }
    if (filp->next != filp) {
        if (xipfs_flash_page_aligned(filp->next) == 0) {
            xipfs_errno = XIPFS_EALIGN;
            return -1;
        }
        if (xipfs_flash_in(filp->next) == 0) {
            xipfs_errno = XIPFS_EOUTNVM;
            return -1;
        }
        if ((uintptr_t)filp >= (uintptr_t)filp->next) {
            xipfs_errno = XIPFS_ELINK;
            return -1;
        }
        if ((uintptr_t)filp + filp->reserved != (uintptr_t)filp->next) {
            xipfs_errno = XIPFS_ELINK;
            return -1;
        }
    }
    if (xipfs_file_path_check(filp->path) < 0) {
        /* xipfs_errno was set */
        return -1;
    }
    if (filp->exec != 0 && filp->exec != 1) {
        xipfs_errno = XIPFS_EPERM;
        return -1;
    }

    return 0;
}

/**
 * @pre vfs_filp must be a pointer to an accessible and valid
 * VFS file structure
 *
 * @brief Retrieves the maximum possible position of a file
 *
 * @param vfs_filp A pointer to a memory region containing a
 * VFS file structure
 *
 * @return Returns the maximum possible position of the file or
 * a negative value otherwise
 */
off_t
xipfs_file_get_max_pos(xipfs_file_t *filp)
{
    off_t max_pos;

    assert(filp != NULL);

    if (xipfs_file_filp_check(filp) < 0) {
        /* xipfs_errno was set */
        return -1;
    }
    max_pos  = (off_t)filp->reserved;
    max_pos -= (off_t)sizeof(*filp);

    return max_pos;
}

/**
 * @pre vfs_filp must be a pointer to an accessible and valid
 * VFS file structure
 *
 * @brief Retrieves the reserved size of a file
 *
 * @param vfs_filp A pointer to a memory region containing a
 * VFS file structure
 *
 * @return Returns the reserved size of the file or a negative
 * value otherwise
 */
off_t
xipfs_file_get_reserved(xipfs_file_t *filp)
{
    off_t reserved;

    assert(filp != NULL);

    if (xipfs_file_filp_check(filp) < 0) {
        /* xipfs_errno was set */
        return -1;
    }
    reserved = (off_t)filp->reserved;

    return reserved;
}

/**
 * @pre filp must be a pointer to an accessible and valid xipfs
 * file structure
 *
 * @brief Removes a file from the file system
 *
 * @param filp A pointer to a memory region containing an
 * accessible xipfs file structure
 *
 * @return Returns zero if the function succeeds or a negative
 * value otherwise
 */
int
xipfs_file_erase(xipfs_file_t *filp)
{
    unsigned start, number, i;

    if (xipfs_file_filp_check(filp) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    start = xipfs_nvm_page(filp);
    number = filp->reserved / XIPFS_NVM_PAGE_SIZE;

    for (i = 0; i < number; i++) {
        if (xipfs_flash_erase_page(start + i) < 0) {
            /* xipfs_errno was set */
            return -1;
        }
    }

    return 0;
}

/**
 * @pre filp must be a pointer to an accessible and valid xipfs
 * file structure
 *
 * @brief Retrieves the current file size from the list of
 * previous sizes
 *
 * @param filp A pointer to a memory region containing an
 * accessible xipfs file structure
 *
 * @return Returns the current file size or a negative value
 * otherwise
 */
off_t
xipfs_file_get_size_(xipfs_file_t *filp)
{
    size_t i = 1;
    off_t size;

    if (filp->size[0] == (size_t)XIPFS_FLASH_ERASE_STATE) {
        /* file size not in flash yet */
        return 0;
    }

    while (i < XIPFS_FILESIZE_SLOT_MAX) {
        if (filp->size[i] == (size_t)XIPFS_FLASH_ERASE_STATE) {
            return (off_t)filp->size[i-1];
        }
        i++;
    }

    if (xipfs_buffer_read_32((unsigned *)&size, &filp->size[i-1]) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    return size;
}

/**
 * @pre vfs_filp must be a pointer to an accessible and valid
 * xipfs file structure
 *
 * @brief Wrapper to the xipfs_file_get_size_ function that
 * checks the validity of the xipfs file strucutre
 *
 * @param vfs_filp A pointer to a memory region containing an
 * accessible xipfs file structure
 *
 * @return Returns the current file size or a negative value
 * otherwise
 */
off_t
xipfs_file_get_size(xipfs_file_t *filp)
{
    if (xipfs_file_filp_check(filp) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    return xipfs_file_get_size_(filp);
}

/**
 * @pre filp must be a pointer to an accessible and valid xipfs
 * file structure
 *
 * @brief Sets the new file size to the list of previous sizes
 *
 * @param vfs_fp A pointer to a memory region containing an
 * accessible xipfs file structure
 *
 * @param size The size to set to the file
 *
 * @return Returns zero if the function succeed or a negative
 * value otherwise
 */
int
xipfs_file_set_size(xipfs_file_t *filp, off_t size)
{
    size_t i = 1;

    if (xipfs_file_filp_check(filp) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    while (i < XIPFS_FILESIZE_SLOT_MAX) {
        if (filp->size[i] != (size_t)XIPFS_FLASH_ERASE_STATE) {
            break;
        }
        i++;
    }
    i %= XIPFS_FILESIZE_SLOT_MAX;

    if (xipfs_buffer_write_32(&filp->size[i], size) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    if (xipfs_buffer_flush() < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    return 0;
}

/**
 * @pre filp must be a pointer to an accessible and valid xipfs
 * file structure
 *
 * @pre path must be a pointer that references a path which is
 * accessible, null-terminated, starts with a slash, normalized,
 * and shorter than XIPFS_PATH_MAX
 *
 * @brief Changes the path of an xipfs file
 *
 * @param filp A pointer to a memory region containing an
 * accessible xipfs file structure
 *
 * @param to_path The new path of the file
 *
 * @return Returns zero if the function succeed or a negative
 * value otherwise
 */
int
xipfs_file_rename(xipfs_file_t *filp, const char *to_path)
{
    size_t len;

    if (xipfs_file_filp_check(filp) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    if (xipfs_file_path_check(to_path) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    len = strlen(to_path) + 1;

    if (xipfs_buffer_write(filp->path, to_path, len) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    if (xipfs_buffer_flush() < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    return 0;
}

/**
 * @pre vfs_filp must be a pointer to an accessible and valid VFS
 * file structure
 *
 * @brief Reads a byte from the current position of the open VFS
 * file
 *
 * @param vfs_filp A pointer to a memory region containing an
 * accessible and open VFS file structure
 *
 * @param byte A pointer to a memory region where to store the
 * read byte
 *
 * @return Returns zero if the function succeed or a negative
 * value otherwise
 */
int
xipfs_file_read_8(xipfs_file_t *filp, off_t pos, char *byte)
{
    off_t pos_max;

    if (xipfs_file_filp_check(filp) < 0) {
        /* xipfs_errno was set */
        return -1;
    }
    if ((pos_max = xipfs_file_get_max_pos(filp)) < 0) {
        /* xipfs_errno was set */
        return -1;
    }
    /* since off_t is defined as a signed integer type, we must
     * verify that the value is non-negative */
    if (pos < 0 || pos > pos_max) {
        xipfs_errno = XIPFS_EMAXOFF;
        return -1;
    }
    if (xipfs_buffer_read_8(byte, &filp->buf[pos]) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    return 0;
}

/**
 * @pre vfs_filp must be a pointer to an accessible and valid VFS
 * file structure
 *
 * @brief Writes a byte from to the current position of the open
 * VFS file
 *
 * @param vfs_filp A pointer to a memory region containing an
 * accessible and open VFS file structure
 *
 * @param byte The byte to write to the current position of the
 * open file
 *
 * @return Returns zero if the function succeed or a negative
 * value otherwise
 */
int
xipfs_file_write_8(xipfs_file_t *filp, off_t pos, char byte)
{
    off_t pos_max;

    if (xipfs_file_filp_check(filp) < 0) {
        /* xipfs_errno was set */
        return -1;
    }
    if ((pos_max = xipfs_file_get_max_pos(filp)) < 0) {
        /* xipfs_errno was set */
        return -1;
    }
    /* since off_t is defined as a signed integer type, we must
     * verify that the value is non-negative */
    if (pos < 0 || pos > pos_max) {
        xipfs_errno = XIPFS_EMAXOFF;
        return -1;
    }
    if (xipfs_buffer_write_8(&filp->buf[pos], byte) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    return 0;
}

/**
 * @pre filp must be a pointer to an accessible and valid xipfs
 * file structure
 *
 * @brief Executes a binary in the current RIOT thread
 *
 * @param filp A pointer to a memory region containing an
 * accessible xipfs file structure
 *
 * @param argv A pointer to a list of pointers to memory regions
 * containing accessible arguments to pass to the binary
 *
 * @return Returns zero if the function succeed or a negative
 * value otherwise
 */
int
xipfs_file_exec(xipfs_file_t *filp, char *const argv[])
{
    static exec_ctx_t exec_ctx;

    if (xipfs_file_filp_check(filp) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    exec_ctx_cleanup(&exec_ctx);
    exec_ctx_init(&exec_ctx, filp, argv);
    xipfs_exec_enter(&exec_ctx.crt0_ctx, filp->buf, exec_ctx.stktop);
    exec_ctx_cleanup(&exec_ctx);

    return 0;
}