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

#ifndef XIPFS_H
#define XIPFS_H

#include <fcntl.h>
#include <stdarg.h>

#ifndef RIOT_VERSION

#include "xipfs_config.h"

#else /* !RIOT_VERSION */

#include "cpu.h"
#include "periph/flashpage.h"
#include "mutex.h"

/**
 * @def XIPFS_PATH_MAX
 *
 * @brief The maximum length of an xipfs path
 */
#define XIPFS_PATH_MAX (64)

/**
 * @def XIPFS_MAGIC
 *
 * @brief The magic number of an xipfs file system
 */
#define XIPFS_MAGIC (0xf9d3b6cbUL)

/**
 * @def XIPFS_FILESIZE_SLOT_MAX
 *
 * @brief The maximum slot number for the list holding file
 * sizes
 */
#define XIPFS_FILESIZE_SLOT_MAX (86)

/**
 * @def XIPFS_EXEC_ARGC_MAX
 *
 * @brief The maximum number of arguments on the command line
 */
#define XIPFS_EXEC_ARGC_MAX (64)

/**
 * @def XIPFS_MAX_OPEN_DESC
 *
 * @brief The maximum number of opened descriptors
 */
#define XIPFS_MAX_OPEN_DESC (16)


/**
 * @def XIPFS_NVM_BASE
 *
 * @brief The non-volatile memory base address
 */
#define XIPFS_NVM_BASE (CPU_FLASH_BASE)

/**
 * @def XIPFS_NVM_ERASE_STATE
 *
 * @brief The non-volatile memory erased state
 */
#define XIPFS_NVM_ERASE_STATE (FLASHPAGE_ERASE_STATE)

/**
 * @def XIPFS_NVM_NUMOF
 *
 * @brief The non-volatile memory flash page number
 */
#define XIPFS_NVM_NUMOF (FLASHPAGE_NUMOF)

/**
 * @def XIPFS_NVM_WRITE_BLOCK_ALIGNMENT
 *
 * @brief The write alignment for the non-volatile memory
 */
#define XIPFS_NVM_WRITE_BLOCK_ALIGNMENT (FLASHPAGE_WRITE_BLOCK_ALIGNMENT)

/**
 * @def XIPFS_NVM_WRITE_BLOCK_SIZE
 *
 * @brief The write size for the non-volatile memory
 */
#define XIPFS_NVM_WRITE_BLOCK_SIZE (FLASHPAGE_WRITE_BLOCK_SIZE)

/**
 * @def XIPFS_NVM_PAGE_SIZE
 *
 * @brief The non-volatile memory flash page size
 */
#define XIPFS_NVM_PAGE_SIZE (FLASHPAGE_SIZE)

#endif /* !RIOT_VERSION */


#ifndef XIPFS_PATH_MAX
#error "xipfs_config.h: XIPFS_PATH_MAX undefined"
#endif /* !XIPFS_PATH_MAX */

#ifndef XIPFS_MAGIC
#error "xipfs_config.h: XIPFS_MAGIC undefined"
#endif /* !XIPFS_MAGIC */

#ifndef XIPFS_FILESIZE_SLOT_MAX
#error "xipfs_config.h: XIPFS_FILESIZE_SLOT_MAX undefined"
#endif /* !XIPFS_FILESIZE_SLOT_MAX */

#ifndef XIPFS_EXEC_ARGC_MAX
#error "xipfs_config.h: XIPFS_EXEC_ARGC_MAX undefined"
#endif /* !XIPFS_EXEC_ARGC_MAX */

#ifndef XIPFS_NVM_BASE
#error "xipfs_config.h: XIPFS_NVM_BASE undefined"
#endif /* !XIPFS_NVM_BASE */

#ifndef XIPFS_NVM_ERASE_STATE
#error "xipfs_config.h: XIPFS_NVM_ERASE_STATE undefined"
#endif /* !XIPFS_NVM_ERASE_STATE */

#ifndef XIPFS_NVM_NUMOF
#error "xipfs_config.h: XIPFS_NVM_NUMOF undefined"
#endif /* !XIPFS_NVM_NUMOF */

#ifndef XIPFS_NVM_WRITE_BLOCK_ALIGNMENT
#error "xipfs_config.h: XIPFS_NVM_WRITE_BLOCK_ALIGNMENT undefined"
#endif /* !XIPFS_NVM_WRITE_BLOCK_ALIGNMENT */

#ifndef XIPFS_NVM_WRITE_BLOCK_SIZE
#error "xipfs_config.h: XIPFS_NVM_WRITE_BLOCK_SIZE undefined"
#endif /* !XIPFS_NVM_WRITE_BLOCK_SIZE */

#ifndef XIPFS_NVM_PAGE_SIZE
#error "xipfs_config.h: XIPFS_NVM_PAGE_SIZE undefined"
#endif /* !XIPFS_NVM_PAGE_SIZE */

#ifndef XIPFS_MAX_OPEN_DESC
#error "xipfs_config.h: XIPFS_MAX_OPEN_DESC undefined"
#endif /* !XIPFS_MAX_OPEN_DESC */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief File data structure for xipfs
 */
typedef struct xipfs_file_s {
    /**
     * The address of the next file
     */
    struct xipfs_file_s *next;
    /**
     * The path of the file relative to the mount point
     */
    char path[XIPFS_PATH_MAX];
    /**
     * The actual size reserved for the file
     */
    size_t reserved;
    /**
     * The table lists the file sizes, with the last entry
     * reflecting the current size of the file. This method
     * helps to avoid flashing the flash page every time there
     * is a change in size
     */
    size_t size[XIPFS_FILESIZE_SLOT_MAX];
    /**
     * Execution right
     */
    uint32_t exec;
    /**
     * First byte of the file's data
     */
    unsigned char buf[0];
} xipfs_file_t;

typedef struct xipfs_mount_s {
    unsigned magic;
    const char *mount_path;
    size_t page_num;
    void *page_addr;
    mutex_t *execution_mutex;
    mutex_t *mutex;
} xipfs_mount_t;

typedef struct xipfs_dir_desc_s {
    xipfs_file_t *filp;
    char dirname[XIPFS_PATH_MAX];
} xipfs_dir_desc_t;

typedef struct xipfs_file_desc_s {
    xipfs_file_t *filp;
    off_t pos;
    int flags;
} xipfs_file_desc_t;

typedef struct xipfs_dirent_s {
    char dirname[XIPFS_PATH_MAX];
} xipfs_dirent_t;

struct xipfs_statvfs {
    unsigned long f_bsize;   /**< File system block size. */
    unsigned long f_frsize;  /**< Fundamental file system block size. */
    fsblkcnt_t f_blocks;     /**< Total number of blocks on file system in
                                  units of @c f_frsize. */
    fsblkcnt_t f_bfree;      /**< Total number of free blocks. */
    fsblkcnt_t f_bavail;     /**< Number of free blocks available to
                                  non-privileged process. */
    unsigned long f_flag;    /**< Bit mask of f_flag values. */
    unsigned long f_namemax; /**< Maximum filename length. */
};

/**
 * @brief Translate the given page number into the page's
 * starting address
 *
 * @note The given page MUST be valid, otherwise the returned
 * address points to an undefined memory location!
 *
 * @param[in] page page number to get the address of
 *
 * @return starting memory address of the given page
 */
void *xipfs_nvm_addr(unsigned page);

/**
 * @brief Erase the given page
 *
 * @param[in] page Page to erase
 */
void xipfs_nvm_erase(unsigned page);

/**
 * @brief Translate the given address into the corresponding
 * page number
 *
 * The given address can be any address inside a page.
 *
 * @note The given address MUST be a valid flash address!
 *
 * @param[in] addr address inside the targeted page
 *
 * @return page containing the given address
 */
unsigned xipfs_nvm_page(const void *addr);

/**
 * @brief Write any number of data bytes to a given location in
 * the flash memory
 *
 * @warning Make sure the targeted memory area is erased before
 * calling this function
 *
 * Both target address and data address must be aligned to
 * FLASHPAGE_BLOCK_ALIGN. @p len must be a multiple of
 * FLASHPAGE_WRITE_BLOCK_SIZE. This function doesn't erase any
 * area in flash, thus be sure the targeted memory area is
 * erased before writing on it (using the flashpage_write
 * function).
 *
 * @param[in] target_addr address in flash to write to. MUST be
 * aligned to FLASHPAGE_WRITE_BLOCK_ALIGNMENT.
 *
 * @param[in] data data to write to the address. MUST be aligned
 * to FLASHPAGE_WRITE_BLOCK_ALIGNMENT.
 *
 * @param[in] len length of the data to be written. It MUST be
 * multiple of FLASHPAGE_WRITE_BLOCK_SIZE. Also, ensure it
 * doesn't exceed the actual flash memory size.
 */
void xipfs_nvm_write(void *target_addr, const void *data, size_t len);

/*
 * xipfs system calls
 */

int xipfs_close(xipfs_mount_t *mp, xipfs_file_desc_t *descp);
int xipfs_closedir(xipfs_mount_t *mp, xipfs_dir_desc_t *descp);

/**
 * @warning The order of the members in the enumeration must
 * remain synchronized with the order of the members of the same
 * enumeration declared in caller site (xipfs_format stdriot's one).
 *
 * @brief An enumeration describing the index of functions.
 * @see xipfs_execv
 */
typedef enum xipfs_user_syscall_e {
    XIPFS_USER_SYSCALL_PRINTF,
    XIPFS_USER_SYSCALL_GET_TEMP,
    XIPFS_USER_SYSCALL_ISPRINT,
    XIPFS_USER_SYSCALL_STRTOL,
    XIPFS_USER_SYSCALL_GET_LED,
    XIPFS_USER_SYSCALL_SET_LED,
    XIPFS_USER_SYSCALL_COPY_FILE,
    XIPFS_USER_SYSCALL_GET_FILE_SIZE,
    XIPFS_USER_SYSCALL_MEMSET,
    XIPFS_USER_SYSCALL_MAX
} xipfs_user_syscall_t;

typedef int (*xipfs_user_syscall_vprintf_t)(const char *format, va_list ap);
typedef int (*xipfs_user_syscall_get_temp_t)(void);
typedef int (*xipfs_user_syscall_isprint_t)(int character);
typedef long (*xipfs_user_syscall_strtol_t)(
    const char *str, char **endptr, int base);
typedef int (*xipfs_user_syscall_get_led_t)(int pos);
typedef int (*xipfs_user_syscall_set_led_t)(int pos, int val);
typedef ssize_t (*xipfs_user_syscall_copy_file_t)(
    const char *name, void *buf, size_t nbyte);
typedef int (*xipfs_user_syscall_get_file_size_t)(
    const char *name, size_t *size);
typedef void *(*syscall_memset_t)(void *m, int c, size_t n);

int xipfs_execv(xipfs_mount_t *mp, const char *full_path, char *const argv[],
                const void *user_syscalls[XIPFS_USER_SYSCALL_MAX]);

int xipfs_format(xipfs_mount_t *mp);
int xipfs_fstat(xipfs_mount_t *mp, xipfs_file_desc_t *descp, struct stat *buf);
int xipfs_fsync(xipfs_mount_t *mp, xipfs_file_desc_t *descp, off_t pos);
off_t xipfs_lseek(xipfs_mount_t *mp, xipfs_file_desc_t *descp, off_t off, int whence);
int xipfs_mkdir(xipfs_mount_t *mp, const char *name, mode_t mode);
int xipfs_mount(xipfs_mount_t *mp);
int xipfs_new_file(xipfs_mount_t *mp, const char *path, uint32_t size, uint32_t exec);
int xipfs_open(xipfs_mount_t *mp, xipfs_file_desc_t *descp, const char *name, int flags, mode_t mode);
int xipfs_opendir(xipfs_mount_t *mp, xipfs_dir_desc_t *descp, const char *dirname);
ssize_t xipfs_read(xipfs_mount_t *mp, xipfs_file_desc_t *descp, void *dest, size_t nbytes);
int xipfs_readdir(xipfs_mount_t *mp, xipfs_dir_desc_t *descp, xipfs_dirent_t *direntp);
int xipfs_rename(xipfs_mount_t *mp, const char *from_path, const char *to_path);
int xipfs_rmdir(xipfs_mount_t *mp, const char *name);
int xipfs_stat(xipfs_mount_t *mp, const char *path, struct stat *buf);
int xipfs_statvfs(xipfs_mount_t *mp, const char *restrict path, struct xipfs_statvfs *restrict buf);
int xipfs_umount(xipfs_mount_t *mp);
int xipfs_unlink(xipfs_mount_t *mp, const char *name);
ssize_t xipfs_write(xipfs_mount_t *mp, xipfs_file_desc_t *descp, const void *src, size_t nbytes);

#ifdef __cplusplus
}
#endif

#endif /* XIPFS_H */

/** @} */
