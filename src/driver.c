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
 * The following define is required in order to use strnlen(3)
 * since glibc 2.10. Refer to the SYNOPSIS section of the
 * strnlen(3) manual and the feature_test_macros(7) manual for
 * more information
 */
#define _POSIX_C_SOURCE 200809L

/*
 * libc includes
 */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

/*
 * xipfs includes
 */
#include "include/buffer.h"
#include "include/desc.h"
#include "include/errno.h"
#include "include/file.h"
#include "include/flash.h"
#include "include/fs.h"
#include "include/path.h"
#include "include/xipfs.h"

/*
 * Macro definitions
 */

/**
 * @internal
 *
 * @def ST_NOSUID
 *
 * @brief The set-user-ID and set-group-ID bits are ignored by
 *        xipfs_execv(3) for executable files on this filesystem
 */
#define ST_NOSUID (2)

/**
 * @internal
 *
 * @def MAX
 *
 * @brief Returns the maximum between x and y
 *
 * @param x The first variable to be compared
 *
 * @param y The second variable to be compared
 */
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

/**
 * @internal
 *
 * @def UNUSED
 *
 * @brief The most versatile macro for disabling warnings
 * related to unused variables
 *
 * @param x The unused variable name
 */
#define UNUSED(x) ((void)(x))

/*
 * Helper functions
 */

/**
 * @internal
 *
 * @pre base must be a pointer that references an accessible
 * memory region
 *
 * @pre path must be a pointer that references a path which is
 * accessible, null-terminated, starts with a slash, normalized,
 * and shorter than XIPFS_PATH_MAX
 *
 * @brief Copy the base name component of path, into the memory
 * region pointed to by base
 *
 * @param base A pointer to a memory region that respects the
 * preconditions for storing the base name component
 *
 * @param path A pointer to a path that respects the
 * preconditions
 */
static void
basename(char *base, const char *path)
{
    const char *ptr, *start, *end;
    size_t len, i;

    assert(base != NULL);
    assert(path != NULL);
    assert(path[0] == '/');

    if (path[1] == '\0') {
        base[0] = '/';
        base[1] = '\0';
        return;
    }

    len = strnlen(path, XIPFS_PATH_MAX);
    assert(len < XIPFS_PATH_MAX);
    ptr = path + len - 1;

    if (ptr[0] == '/') {
        /* skip the trailing slash if the
         * path ends with one */
        ptr--;
    }
    end = ptr;

    while (ptr > path && *ptr != '/') {
        /* skip all characters that are not
         * slashes */
        ptr--;
    }
    /* skip the slash */
    start = ptr + 1;

    for (i = 0; start + i <= end; i++) {
        /* copy the characters of the base
         * name component until end */
        base[i] = start[i];
    }
    base[i] = '\0';
}

/**
 * @internal
 *
 * @pre xipfs_mp must be a pointer that references a memory
 * region containing an xipfs mount point structure which is
 * accessible and valid
 *
 * @pre xipfs_filp must be a pointer that references an
 * accessible memory region
 *
 * @brief Remove a file by flushing the read/write buffer,
 * consolidating the file system, and updating the internal
 * xipfs file addresses of all open VFS file descriptor
 * structures
 *
 * @param xipfs_mp A pointer to a memory region containing an
 * xipfs mount point structure
 *
 * @param xipfs_filp A pointer to a memory region containing
 * the accessible xipfs file structure of the xipfs file to
 * remove
 *
 * @return Returns zero if the function succeeds or a negative
 * value otherwise
 */
static int
sync_remove_file(xipfs_mount_t *mp, xipfs_file_t *filp)
{
    size_t reserved;

    assert(mp != NULL);
    assert(filp != NULL);

    if (xipfs_buffer_flush() < 0) {
        return -1;
    }
    reserved = filp->reserved;
    if (xipfs_fs_remove(filp) < 0) {
        return -1;
    }
    xipfs_desc_update(mp, filp, reserved);

    return 0;
}

/**
 * @internal
 *
 * @brief Checks if the xipfs mount point structure passed as
 * an argument is valid
 *
 * @param xipfs_mp A pointer to a memory region containing an
 * xipfs mount point structure
 *
 * @return Returns zero if the xipfs mount point structure
 * passed as an argument is valid or a negative value otherwise
 */
static int
xipfs_mp_check(xipfs_mount_t *mp)
{
    uint32_t page;

    if (mp == NULL) {
        return -EFAULT;
    }
    if (mp->magic != XIPFS_MAGIC) {
        return -EINVAL;
    }
    if (!xipfs_flash_in(mp->page_addr)) {
        return -EINVAL;
    }
    if (mp->page_num == 0) {
        return -EINVAL;
    }
    if (mp->page_num > XIPFS_NVM_NUMOF) {
        return -EINVAL;
    }
    page = xipfs_nvm_page(mp->page_addr);
    if (page + mp->page_num > XIPFS_NVM_NUMOF) {
        return -EINVAL;
    }

    return 0;
}

/**
 * @internal
 *
 * @pre should be call after xipfs_mp_check
 */
static int
xipfs_file_desc_check(xipfs_mount_t *mp, xipfs_file_desc_t *descp)
{
    uintptr_t start, end, filp;

    start = (uintptr_t)mp->page_addr;
    end = start + mp->page_num * XIPFS_NVM_PAGE_SIZE;

    if (descp == NULL) {
        return -EFAULT;
    }
    if (descp->filp == NULL) {
        return -EINVAL;
    }
    filp = (uintptr_t)descp->filp;
    if (!(filp >= start && filp < end)) {
        return -EINVAL;
    }
    if (descp->pos < 0) {
        return -EINVAL;
    }
    if (!((descp->flags & O_CREAT)  == O_CREAT  ||
          (descp->flags & O_EXCL)   == O_EXCL   ||
          (descp->flags & O_WRONLY) == O_WRONLY ||
          (descp->flags & O_RDONLY) == O_RDONLY ||
          (descp->flags & O_RDWR)   == O_RDWR   ||
          (descp->flags & O_APPEND) == O_APPEND))
    {
        return -EINVAL;
    }

    return 0;
}

/*
 * Operations on open files
 */

int
xipfs_close(xipfs_mount_t *mp, xipfs_file_desc_t *descp)
{
    off_t size;
    int ret;

    if ((ret = xipfs_mp_check(mp)) < 0) {
        return ret;
    }
    if ((uintptr_t)descp->filp == (uintptr_t)xipfs_infos_file) {
        /* nothing to do */
        return 0;
    }
    if ((ret = xipfs_file_desc_check(mp, descp)) < 0) {
        return ret;
    }
    if ((ret = xipfs_file_desc_tracked(descp)) < 0) {
        return ret;
    }
    if ((size = xipfs_file_get_size(descp->filp)) < 0) {
        return -EIO;
    }
    if (size < descp->pos) {
        /* synchronise file size */
        if (xipfs_file_set_size(descp->filp, descp->pos) < 0) {
            return -EIO;
        }
    }
    if ((ret = xipfs_file_desc_untrack(descp)) < 0) {
        return ret;
    }

    return 0;
}

int
xipfs_fstat(xipfs_mount_t *mp, xipfs_file_desc_t *descp,
            struct stat *buf)
{
    off_t size, reserved;
    int ret;

    if (buf == NULL) {
        return -EFAULT;
    }
    if ((ret = xipfs_mp_check(mp)) < 0) {
        return ret;
    }
    if ( (descp != NULL) &&
         ((uintptr_t)descp->filp == (uintptr_t)xipfs_infos_file) ) {
        /* cannot fstat(2) */
        return -EBADF;
    }
    if ((ret = xipfs_file_desc_check(mp, descp)) < 0) {
        return ret;
    }
    if ((ret = xipfs_file_desc_tracked(descp)) < 0) {
        return ret;
    }
    if ((size = xipfs_file_get_size(descp->filp)) < 0) {
        return -EIO;
    }
    if ((reserved = xipfs_file_get_reserved(descp->filp)) < 0) {
        return -EIO;
    }

    (void)memset(buf, 0, sizeof(*buf));
    buf->st_dev = (dev_t)(uintptr_t)mp;
    buf->st_ino = (ino_t)(uintptr_t)descp->filp;
    buf->st_mode = S_IFREG;
    buf->st_nlink = 1;
    buf->st_size = MAX(size, descp->pos);
    buf->st_blksize = XIPFS_NVM_PAGE_SIZE;
    buf->st_blocks = reserved / XIPFS_NVM_PAGE_SIZE;

    return 0;
}

off_t
xipfs_lseek(xipfs_mount_t *mp, xipfs_file_desc_t *descp,
            off_t off, int whence)
{
    off_t max_pos, new_pos, size;
    int ret;

    UNUSED(mp);

    if ((ret = xipfs_mp_check(mp)) < 0) {
        return ret;
    }
    if ((ret = xipfs_file_desc_check(mp, descp)) < 0) {
        return ret;
    }
    if ((ret = xipfs_file_desc_tracked(descp)) < 0) {
        return -EBADF;
    }
    if ((uintptr_t)descp->filp != (uintptr_t)xipfs_infos_file) {
        if ((max_pos = xipfs_file_get_max_pos(descp->filp)) < 0) {
            return -EIO;
        }
        if ((size = xipfs_file_get_size(descp->filp)) < 0) {
            return -EIO;
        }
    } else {
        max_pos = sizeof(xipfs_mount_t);
        size = (off_t)sizeof(xipfs_mount_t);
    }

    switch (whence) {
        case SEEK_SET:
            new_pos = off;
            break;
        case SEEK_CUR:
            new_pos = descp->pos + off;
            break;
        case SEEK_END:
            new_pos = MAX(descp->pos, size) + off;
            break;
        default:
            return -EINVAL;
    }
    if (new_pos < 0 || new_pos > max_pos) {
        return -EINVAL;
    }
    if (descp->pos > size && new_pos < descp->pos) {
        /* synchronise file size */
        if (xipfs_file_set_size(descp->filp, descp->pos) < 0) {
            return -EIO;
        }
    }
    descp->pos = new_pos;

    return new_pos;
}

int
xipfs_open(xipfs_mount_t *mp, xipfs_file_desc_t *descp,
           const char *name, int flags, mode_t mode)
{
    char buf[XIPFS_PATH_MAX];
    xipfs_path_t xipath;
    xipfs_file_t *filp;
    size_t len;
    off_t pos;
    int ret;

    /* mode bits are ignored */
    UNUSED(mode);

    if ((ret = xipfs_mp_check(mp)) < 0) {
        return ret;
    }
    if (descp == NULL) {
        return -EFAULT;
    }
    if (name == NULL) {
        return -EFAULT;
    }

    /* only these flags are supported */
#define XIPFS_SUPPORTED_FLAGS   \
(                               \
    O_CREAT | O_EXCL | O_APPEND \
)
    const int flags_acc_mode = flags & O_ACCMODE;
    const int flags_without_acc_mode = flags & (~O_ACCMODE);
    switch(flags_acc_mode) {
        case O_RDONLY :
            /* fallthrough */
        case O_WRONLY :
            /* fallthrough */
        case O_RDWR :
            if (   ( (flags_without_acc_mode & ~XIPFS_SUPPORTED_FLAGS) != 0 )
                && ( (flags_without_acc_mode & XIPFS_SUPPORTED_FLAGS)  == 0 ) )
                return -EINVAL;
            break;
        default :
            // Should not happen
            return -EINVAL;
    }
#undef XIPFS_SUPPORTED_FLAGS

    len = strnlen(name, XIPFS_PATH_MAX);
    if (len == XIPFS_PATH_MAX) {
        return -ENAMETOOLONG;
    }

    /* virtual file handling */
    basename(buf, name);
    if (strncmp(buf, ".xipfs_infos", XIPFS_PATH_MAX) == 0) {
        if ((flags & O_CREAT) == O_CREAT &&
            (flags & O_EXCL) == O_EXCL) {
            return -EEXIST;
        }
        if ((flags & O_WRONLY) == O_WRONLY ||
            (flags & O_APPEND) == O_APPEND ||
            (flags & O_RDWR) == O_RDWR) {
            return -EACCES;
        }
        descp->filp = (void *)xipfs_infos_file;
        descp->flags = flags;
        descp->pos = 0;
        return 0;
    }

    if (xipfs_path_new(mp, &xipath, name) < 0) {
        return -EIO;
    }
    switch (xipath.info) {
    case XIPFS_PATH_EXISTS_AS_FILE:
        if ((flags & O_CREAT) == O_CREAT &&
            (flags & O_EXCL) == O_EXCL) {
            return -EEXIST;
        }
        filp = xipath.witness;
        break;
    case XIPFS_PATH_EXISTS_AS_EMPTY_DIR:
    case XIPFS_PATH_EXISTS_AS_NONEMPTY_DIR:
        return -EISDIR;
    case XIPFS_PATH_INVALID_BECAUSE_NOT_DIRS:
        return -ENOTDIR;
    case XIPFS_PATH_INVALID_BECAUSE_NOT_FOUND:
        return -ENOENT;
    case XIPFS_PATH_CREATABLE:
        if ((flags & O_CREAT) != O_CREAT) {
            return -ENOENT;
        }
        if (xipath.path[xipath.len-1] == '/') {
            return -EISDIR;
        }
        if (xipath.witness != NULL && !(xipath.dirname[0] == '/' &&
                xipath.dirname[1] == '\0')) {
            if (strcmp(xipath.witness->path, xipath.dirname) == 0) {
                if (sync_remove_file(mp, xipath.witness) < 0) {
                    return -EIO;
                }
            }
        }
        if ((filp = xipfs_fs_new_file(mp, name, 0, 0)) == NULL) {
            /* file creation failed */
            if (xipfs_errno == XIPFS_ENOSPACE ||
                xipfs_errno == XIPFS_EFULL) {
                return -EDQUOT;
            }
            return -EIO;
        }
        break;
    default:
        return -EIO;
    }
    if ((flags & O_APPEND) == O_APPEND) {
        if ((pos = xipfs_file_get_size(filp)) < 0) {
            return -EIO;
        }
    } else {
        pos = 0;
    }

    if ((ret = xipfs_file_desc_track(descp)) < 0) {
        return ret;
    }
    descp->filp = filp;
    descp->flags = flags;
    descp->pos = pos;

    return 0;
}

ssize_t
xipfs_read(xipfs_mount_t *mp, xipfs_file_desc_t *descp,
           void *dest, size_t nbytes)
{
    off_t size;
    size_t i;
    int ret;

    /** Special case : virtual file
     * This code is used to retrieve the xipfs_mount_t where it is not provided
     */
    if ( (descp != NULL) && ((uintptr_t)descp->filp == (uintptr_t)xipfs_infos_file) ) {
        for (i = 0; i < nbytes && i < sizeof(xipfs_mount_t); i++) {
            ((char *)dest)[i] = ((char *)mp)[i];
        }
        return i;
    }

    if ((ret = xipfs_mp_check(mp)) < 0) {
        return ret;
    }
    if ((ret = xipfs_file_desc_check(mp, descp)) < 0) {
        return ret;
    }
    if ((ret = xipfs_file_desc_tracked(descp)) < 0) {
        return ret;
    }
    if (dest == NULL) {
        return -EFAULT;
    }
    switch(descp->flags & O_ACCMODE) {
        case O_RDONLY :
            /* fallthrough */
        case O_RDWR :
            break;
        default :
            return -EACCES;
    }
    if ((size = xipfs_file_get_size(descp->filp)) < 0) {
        return -EIO;
    }
    for (i = 0; i < nbytes && descp->pos < size; i++) {
        if (xipfs_file_read_8(descp->filp, descp->pos,
                &((char *)dest)[i]) < 0) {
            return -EIO;
        }
        descp->pos++;
    }

    return i;
}

ssize_t
xipfs_write(xipfs_mount_t *mp, xipfs_file_desc_t *descp,
            const void *src, size_t nbytes)
{
    off_t max_pos;
    size_t i;
    int ret;

    if ((ret = xipfs_mp_check(mp)) < 0) {
        return ret;
    }
    if ((ret = xipfs_file_desc_check(mp, descp)) < 0) {
        return ret;
    }
    if ((ret = xipfs_file_desc_tracked(descp)) < 0) {
        return -EBADF;
    }
    if (src == NULL) {
        return -EFAULT;
    }
    if (((descp->flags & O_WRONLY) != O_WRONLY) &&
        ((descp->flags & O_RDWR) != O_RDWR)) {
        return -EACCES;
    }
    if ((uintptr_t)descp->filp == (uintptr_t)xipfs_infos_file) {
        /* cannot write(2) */
        return -EBADF;
    }
    if ((max_pos = xipfs_file_get_max_pos(descp->filp)) < 0) {
        return -EIO;
    }
    for (i = 0; i < nbytes && descp->pos < max_pos; i++) {
        if (xipfs_file_write_8(descp->filp, descp->pos,
                ((char *)src)[i]) < 0) {
            return -EIO;
        }
        descp->pos++;
    }

    return i;
}

int
xipfs_fsync(xipfs_mount_t *mp, xipfs_file_desc_t *descp,
            off_t pos)
{
    int ret;

    if ((ret = xipfs_mp_check(mp)) < 0) {
        return ret;
    }
    if ((ret = xipfs_file_desc_check(mp, descp)) < 0) {
        return ret;
    }
    if ((ret = xipfs_file_desc_tracked(descp)) < 0) {
        return ret;
    }
    if (((descp->flags & O_WRONLY) != O_WRONLY) &&
        ((descp->flags & O_RDWR) != O_RDWR)) {
        return -EACCES;
    }
    if (xipfs_file_set_size(descp->filp, pos) < 0) {
        return -EIO;
    }

    return 0;
}

/*
 * Operations on open directories
 */

int
xipfs_opendir(xipfs_mount_t *mp, xipfs_dir_desc_t *descp,
              const char *dirname)
{
    xipfs_path_t xipath;
    xipfs_file_t *headp;
    size_t len;
    int ret;

    if ((ret = xipfs_mp_check(mp)) < 0) {
        return ret;
    }
    if (descp == NULL) {
        return -EFAULT;
    }
    if (dirname == NULL) {
        return -EFAULT;
    }
    if (dirname[0] == '\0') {
        return -ENOENT;
    }
    len = strnlen(dirname, XIPFS_PATH_MAX);
    if (len == XIPFS_PATH_MAX) {
        return -ENAMETOOLONG;
    }

    xipfs_errno = XIPFS_OK;
    headp = xipfs_fs_head(mp);
    if (xipfs_errno != XIPFS_OK) {
        return -EIO;
    }
    if ( (headp == NULL) && (dirname[0] == '/' && dirname[1] == '\0') ) {
        /* this file system is empty, not an error
         * the root of the file system is always present
         */
        if ((ret = xipfs_dir_desc_track(descp)) < 0) {
            return ret;
        }
        descp->dirname[0] = '/';
        descp->dirname[1] = '\0';
        descp->filp = headp;
        return 0;
    }

    if (xipfs_path_new(mp, &xipath, dirname) < 0) {
        return -EIO;
    }
    switch (xipath.info) {
    case XIPFS_PATH_EXISTS_AS_FILE:
    case XIPFS_PATH_INVALID_BECAUSE_NOT_DIRS:
        return -ENOTDIR;
    case XIPFS_PATH_EXISTS_AS_EMPTY_DIR:
    case XIPFS_PATH_EXISTS_AS_NONEMPTY_DIR:
        break;
    case XIPFS_PATH_INVALID_BECAUSE_NOT_FOUND:
    case XIPFS_PATH_CREATABLE:
    case XIPFS_PATH_UNDEFINED:
        return -ENOENT;
    default:
        return -EIO;
    }

    if ((ret = xipfs_dir_desc_track(descp)) < 0) {
        return ret;
    }

    /* it is safe to use strcpy(3) here */
    (void)strcpy(descp->dirname, dirname);
    descp->filp = headp;

    len = xipath.len;
    if (descp->dirname[len-1] != '/') {
        if (len+1 == XIPFS_PATH_MAX) {
            return -ENAMETOOLONG;
        }
        /* ensure dirname ends with a slash to indicate it's a
         * directory */
        descp->dirname[len] = '/';
        descp->dirname[len+1] = '\0';
    }

    return 0;
}

int
xipfs_readdir(xipfs_mount_t *mp, xipfs_dir_desc_t *descp,
              xipfs_dirent_t *direntp)
{
    size_t i, j;
    int ret;

    if ((ret = xipfs_mp_check(mp)) < 0) {
        return ret;
    }
    /* TODO check descp integrity */
    if (descp == NULL) {
        return -EFAULT;
    }
    if (direntp == NULL) {
        return -EFAULT;
    }
    if ((ret = xipfs_dir_desc_tracked(descp)) < 0) {
        return ret;
    }

    xipfs_errno = XIPFS_OK;
    while (descp->filp != NULL) {
        i = 0;
        while (i < XIPFS_PATH_MAX) {
            if (descp->filp->path[i] != descp->dirname[i]) {
                break;
            }
            if (descp->dirname[i] == '\0') {
                break;
            }
            if (descp->filp->path[i] == '\0') {
                break;
            }
            i++;
        }
        if (i == XIPFS_PATH_MAX) {
            return -ENAMETOOLONG;
        }
        if (descp->dirname[i] == '\0') {
            if (descp->filp->path[i] == '/') {
                /* skip first slash */
                i++;
            }
            j = i;
            while (j < XIPFS_PATH_MAX) {
                if (descp->filp->path[j] == '\0') {
                    direntp->dirname[j-i] = '\0';
                    break;
                }
                if (descp->filp->path[j] == '/') {
                    direntp->dirname[j-i] = '/';
                    direntp->dirname[j-i+1] = '\0';
                    break;
                }
                direntp->dirname[j-i] = descp->filp->path[j];
                j++;
            }
            if (j == XIPFS_PATH_MAX) {
                return -ENAMETOOLONG;
            }
            /* set the next file to the structure */
            if ((descp->filp = xipfs_fs_next(descp->filp)) == NULL) {
                if (xipfs_errno != XIPFS_OK) {
                    return -EIO;
                }
            }
            /* entry was updated */
            return 1;
        }
        descp->filp = xipfs_fs_next(descp->filp);
    }
    if (xipfs_errno != XIPFS_OK) {
        return -EIO;
    }
    /* end of the directory */
    return 0;
}

int
xipfs_closedir(xipfs_mount_t *mp, xipfs_dir_desc_t *descp)
{
    int ret;

    if ((ret = xipfs_mp_check(mp)) < 0) {
        return ret;
    }
    /* XXX check descp integrity */
    if (descp == NULL) {
        return -EFAULT;
    }
    if ((ret = xipfs_dir_desc_tracked(descp)) < 0) {
        return ret;
    }
    (void)memset(descp, 0, sizeof(*descp));
    if ((ret = xipfs_dir_desc_untrack(descp)) < 0) {
        return ret;
    }

    return 0;
}

/*
 * Operations on mounted file systems
 */

int
xipfs_format(xipfs_mount_t *mp)
{
    int ret;

    if ((ret = xipfs_mp_check(mp)) < 0) {
        return ret;
    }
    if (xipfs_fs_format(mp) < 0) {
        return -EIO;
    }
    if ((ret = xipfs_desc_untrack_all(mp)) < 0) {
        return ret;
    }

    return 0;
}

int
xipfs_mount(xipfs_mount_t *mp)
{
    int *start, *end;
    int ret;

    if ((ret = xipfs_mp_check(mp)) < 0) {
        return ret;
    }
    /* check file system integrity using last file pointer */
    xipfs_errno = XIPFS_OK;
    if (xipfs_fs_tail(mp) == NULL) {
        if (xipfs_errno != XIPFS_OK) {
            return -EIO;
        }
    }
    /* ensure pages after the last file are erased */
    if ((start = (int *)xipfs_fs_tail_next(mp)) == NULL) {
        if (xipfs_errno != XIPFS_OK) {
            return -EIO;
        }
    }
    end = (int *)((uintptr_t)mp->page_addr + mp->page_num *
        XIPFS_NVM_PAGE_SIZE);
    while (start < end) {
        if (*start++ != (int)XIPFS_FLASH_ERASE_STATE) {
            return -EIO;
        }
    }

    return 0;
}

int
xipfs_umount(xipfs_mount_t *mp)
{
    int ret;

    if ((ret = xipfs_mp_check(mp)) < 0) {
        return ret;
    }
    if ((ret = xipfs_desc_untrack_all(mp)) < 0) {
        return ret;
    }

    return 0;
}

int
xipfs_unlink(xipfs_mount_t *mp, const char *name)
{
    xipfs_path_t xipath;
    size_t len;
    int ret;

    if ((ret = xipfs_mp_check(mp)) < 0) {
        return ret;
    }
    if (name == NULL) {
        return -EFAULT;
    }
    if (name[0] == '\0') {
        return -ENOENT;
    }
    if (name[0] == '/' && name[1] == '\0') {
        return -EISDIR;
    }
    len = strnlen(name, XIPFS_PATH_MAX);
    if (len == XIPFS_PATH_MAX) {
        return -ENAMETOOLONG;
    }

    if (xipfs_path_new(mp, &xipath, name) < 0) {
        return -EIO;
    }
    switch (xipath.info) {
    case XIPFS_PATH_EXISTS_AS_FILE:
        break;
    case XIPFS_PATH_EXISTS_AS_EMPTY_DIR:
    case XIPFS_PATH_EXISTS_AS_NONEMPTY_DIR:
        return -EISDIR;
    case XIPFS_PATH_INVALID_BECAUSE_NOT_DIRS:
        return -ENOTDIR;
    case XIPFS_PATH_INVALID_BECAUSE_NOT_FOUND:
    case XIPFS_PATH_CREATABLE:
        return -ENOENT;
    default:
        return -EIO;
    }

    if (sync_remove_file(mp, xipath.witness) < 0) {
        return -EIO;
    }
    if (xipath.parent == 1 && !(xipath.dirname[0] ==
            '/' && xipath.dirname[1] == '\0')) {
        if (xipfs_fs_new_file(mp, xipath.dirname,
                XIPFS_NVM_PAGE_SIZE, 0) == NULL) {
            return -EIO;
        }
    }

    return 0;
}

int
xipfs_mkdir(xipfs_mount_t *mp, const char *name, mode_t mode)
{
    xipfs_path_t xipath;
    int ret;

    /* mode bits are ignored */
    UNUSED(mode);

    if ((ret = xipfs_mp_check(mp)) < 0) {
        return ret;
    }
    if (name == NULL) {
        return -EFAULT;
    }
    if (name[0] == '\0') {
        return -ENOENT;
    }
    if (name[0] == '/' && name[1] == '\0') {
        return -EEXIST;
    }
    if (strnlen(name, XIPFS_PATH_MAX) == XIPFS_PATH_MAX) {
        return -ENAMETOOLONG;
    }

    if (xipfs_path_new(mp, &xipath, name) < 0) {
        return -EIO;
    }
    switch (xipath.info) {
    case XIPFS_PATH_EXISTS_AS_FILE:
    case XIPFS_PATH_EXISTS_AS_EMPTY_DIR:
    case XIPFS_PATH_EXISTS_AS_NONEMPTY_DIR:
        return -EEXIST;
    case XIPFS_PATH_INVALID_BECAUSE_NOT_DIRS:
        return -ENOTDIR;
    case XIPFS_PATH_INVALID_BECAUSE_NOT_FOUND:
        return -ENOENT;
    case XIPFS_PATH_CREATABLE:
        break;
    default:
        return -EIO;
    }

    if (xipath.path[xipath.len-1] != '/') {
        if (xipath.len == XIPFS_PATH_MAX-1) {
            return -ENAMETOOLONG;
        }
        xipath.path[xipath.len++] = '/';
        xipath.path[xipath.len  ] = '\0';
    }

    if (xipath.witness != NULL) {
        if (strcmp(xipath.witness->path, xipath.dirname) == 0) {
            if (sync_remove_file(mp, xipath.witness) < 0) {
                return -EIO;
            }
        }
    }
    if (xipfs_fs_new_file(mp, xipath.path,
            XIPFS_NVM_PAGE_SIZE, 0) == NULL) {
        return -EIO;
    }

    return 0;
}

int
xipfs_rmdir(xipfs_mount_t *mp, const char *name)
{
    xipfs_path_t xipath;
    size_t len;
    int ret;

    if ((ret = xipfs_mp_check(mp)) < 0) {
        return ret;
    }
    if (name == NULL) {
        return -EFAULT;
    }
    if (name[0] == '\0') {
        return -ENOENT;
    }
    if (name[0] == '/' && name[1] == '\0') {
        return -EBUSY;
    }
    len = strnlen(name, XIPFS_PATH_MAX);
    if (len == XIPFS_PATH_MAX) {
        return -ENAMETOOLONG;
    }
    if (name[len-1] == '.') {
        return -EINVAL;
    }

    if (xipfs_path_new(mp, &xipath, name) < 0) {
        return -EIO;
    }
    switch (xipath.info) {
    case XIPFS_PATH_EXISTS_AS_FILE:
    case XIPFS_PATH_INVALID_BECAUSE_NOT_DIRS:
        return -ENOTDIR;
    case XIPFS_PATH_EXISTS_AS_EMPTY_DIR:
        break;
    case XIPFS_PATH_EXISTS_AS_NONEMPTY_DIR:
        return -ENOTEMPTY;
    case XIPFS_PATH_INVALID_BECAUSE_NOT_FOUND:
    case XIPFS_PATH_CREATABLE:
        return -ENOENT;
    default:
        return -EIO;
    }

    if (sync_remove_file(mp, xipath.witness) < 0) {
        return -EIO;
    }
    if (xipath.parent == 1 && !(xipath.dirname[0] ==
            '/' && xipath.dirname[1] == '\0')) {
        if (xipfs_fs_new_file(mp, xipath.dirname,
                XIPFS_NVM_PAGE_SIZE, 0) == NULL) {
            return -EIO;
        }
    }

    return 0;
}

int
xipfs_rename(xipfs_mount_t *mp, const char *from_path,
             const char *to_path)
{
    xipfs_path_t xipaths[2];
    const char *paths[2];
    size_t renamed;
    ssize_t ret;

    if ((ret = xipfs_mp_check(mp)) < 0) {
        return ret;
    }
    if (from_path == NULL) {
        return -EFAULT;
    }
    if (to_path == NULL) {
        return -EFAULT;
    }
    if (from_path[0] == '\0') {
        return -ENOENT;
    }
    if (to_path[0] == '\0') {
        return -ENOENT;
    }
    ret = strnlen(from_path, XIPFS_PATH_MAX);
    if (ret == XIPFS_PATH_MAX) {
        return -ENAMETOOLONG;
    }
    ret = strnlen(to_path, XIPFS_PATH_MAX);
    if (ret == XIPFS_PATH_MAX) {
        return -ENAMETOOLONG;
    }

    paths[0] = from_path;
    paths[1] = to_path;
    if (xipfs_path_new_n(mp, xipaths, paths, 2) < 0) {
        return -EIO;
    }

    switch (xipaths[0].info) {
    case XIPFS_PATH_EXISTS_AS_FILE:
    {
        switch (xipaths[1].info) {
        case XIPFS_PATH_EXISTS_AS_FILE:
        {
            if (xipaths[0].witness == xipaths[1].witness) {
                return 0;
            }
            if (xipfs_file_rename(xipaths[0].witness,
                    xipaths[1].path) < 0) {
                return -EIO;
            }
            renamed = 1;
            break;
        }
        case XIPFS_PATH_EXISTS_AS_EMPTY_DIR:
        case XIPFS_PATH_EXISTS_AS_NONEMPTY_DIR:
            return -EISDIR;
        case XIPFS_PATH_INVALID_BECAUSE_NOT_DIRS:
            return -ENOTDIR;
        case XIPFS_PATH_INVALID_BECAUSE_NOT_FOUND:
            return -ENOENT;
        case XIPFS_PATH_CREATABLE:
        {
            if (xipaths[1].path[xipaths[1].len-1] == '/') {
                return -ENOTDIR;
            }
            if (xipfs_file_rename(xipaths[0].witness,
                    xipaths[1].path) < 0) {
                return -EIO;
            }
            renamed = 1;
            break;
        }
        default:
            return -EIO;
        }
        break;
    }
    case XIPFS_PATH_EXISTS_AS_EMPTY_DIR:
    {
        switch (xipaths[1].info) {
        case XIPFS_PATH_EXISTS_AS_FILE:
            return -ENOTDIR;
        case XIPFS_PATH_EXISTS_AS_EMPTY_DIR:
        {
            if (xipaths[0].witness == xipaths[1].witness) {
                return 0;
            }
            if (xipfs_file_rename(xipaths[0].witness,
                    xipaths[1].path) < 0) {
                return -EIO;
            }
            renamed = 1;
            break;
        }
        case XIPFS_PATH_EXISTS_AS_NONEMPTY_DIR:
            return -ENOTEMPTY;
        case XIPFS_PATH_INVALID_BECAUSE_NOT_DIRS:
            return -ENOTDIR;
        case XIPFS_PATH_INVALID_BECAUSE_NOT_FOUND:
            return -ENOENT;
        case XIPFS_PATH_CREATABLE:
        {
            /* from is an empty directory */
            if (xipaths[1].path[xipaths[1].len-1] != '/') {
                if (xipaths[1].len == XIPFS_PATH_MAX-1) {
                    return -ENAMETOOLONG;
                }
                xipaths[1].path[xipaths[1].len++] = '/';
                xipaths[1].path[xipaths[1].len  ] = '\0';
            }
            /* check whether an attempt was made to make a
             * directory a subdirectory of itself */
            if (strncmp(xipaths[0].path, xipaths[1].path,
                    xipaths[0].len) == 0) {
                return -EINVAL;
            }
            if (xipfs_file_rename(xipaths[0].witness,
                    xipaths[1].path) < 0) {
                return -EIO;
            }
            renamed = 1;
            break;
        }
        default:
            return -EIO;
        }
        break;
    }
    case XIPFS_PATH_EXISTS_AS_NONEMPTY_DIR:
    {
        switch (xipaths[1].info) {
        case XIPFS_PATH_EXISTS_AS_FILE:
            return -ENOTDIR;
        case XIPFS_PATH_EXISTS_AS_EMPTY_DIR:
        {
            /* check whether an attempt was made to make a
             * directory a subdirectory of itself */
            if (strncmp(xipaths[0].path, xipaths[1].path,
                    xipaths[0].len) == 0) {
                return -EINVAL;
            }
            if ((ret = xipfs_fs_rename_all(mp, xipaths[0].path,
                    xipaths[1].path)) < 0) {
                return -EIO;
            }
            renamed = (size_t)ret;
            break;
        }
        case XIPFS_PATH_EXISTS_AS_NONEMPTY_DIR:
            return -ENOTEMPTY;
        case XIPFS_PATH_INVALID_BECAUSE_NOT_DIRS:
            return -ENOTDIR;
        case XIPFS_PATH_INVALID_BECAUSE_NOT_FOUND:
            return -ENOENT;
        case XIPFS_PATH_CREATABLE:
        {
            /* from is an nonempty directory */
            if (xipaths[1].path[xipaths[1].len-1] != '/') {
                if (xipaths[1].len == XIPFS_PATH_MAX-1) {
                    return -ENAMETOOLONG;
                }
                xipaths[1].path[xipaths[1].len++] = '/';
                xipaths[1].path[xipaths[1].len  ] = '\0';
            }
            /* check whether an attempt was made to make a
             * directory a subdirectory of itself */
            if (strncmp(xipaths[0].path, xipaths[1].path,
                    xipaths[0].len) == 0) {
                return -EINVAL;
            }
            if ((ret = xipfs_fs_rename_all(mp, xipaths[0].path,
                    xipaths[1].path)) < 0) {
                return -EIO;
            }
            renamed = (size_t)ret;
            break;
        }
        default:
            return -EIO;
        }
        break;
    }
    case XIPFS_PATH_INVALID_BECAUSE_NOT_DIRS:
        return -ENOTDIR;
    case XIPFS_PATH_INVALID_BECAUSE_NOT_FOUND:
    case XIPFS_PATH_CREATABLE:
        return -ENOENT;
    default:
        return -EIO;
    }

    if (xipaths[0].parent == renamed && !(xipaths[0].dirname[0] ==
            '/' && xipaths[0].dirname[1] == '\0')) {
        if (strcmp(xipaths[0].dirname, xipaths[1].dirname) != 0) {
            if (xipfs_fs_new_file(mp, xipaths[0].dirname,
                    XIPFS_NVM_PAGE_SIZE, 0) == NULL) {
                return -EIO;
            }
        }
    }

    if (xipaths[1].witness != NULL) {
        if (strcmp(xipaths[1].witness->path, xipaths[1].dirname) == 0) {
            if (sync_remove_file(mp, xipaths[1].witness) < 0) {
                return -EIO;
            }
        }
    }

    return 0;
}

int
xipfs_stat(xipfs_mount_t *mp, const char *path,
           struct stat *buf)
{
    xipfs_path_t xipath;
    size_t len;
    off_t size;
    int ret;

    if ((ret = xipfs_mp_check(mp)) < 0) {
        return ret;
    }
    if (path == NULL) {
        return -EFAULT;
    }
    if (buf == NULL) {
        return -EFAULT;
    }
    if (path[0] == '\0') {
        return -ENOENT;
    }
    len = strnlen(path, XIPFS_PATH_MAX);
    if (len == XIPFS_PATH_MAX) {
        return -ENAMETOOLONG;
    }

    if (xipfs_path_new(mp, &xipath, path) < 0) {
        return -EIO;
    }
    switch (xipath.info) {
    case XIPFS_PATH_EXISTS_AS_FILE:
    case XIPFS_PATH_EXISTS_AS_EMPTY_DIR:
    case XIPFS_PATH_EXISTS_AS_NONEMPTY_DIR:
        break;
    case XIPFS_PATH_INVALID_BECAUSE_NOT_DIRS:
        return -ENOTDIR;
    case XIPFS_PATH_INVALID_BECAUSE_NOT_FOUND:
    case XIPFS_PATH_CREATABLE:
        return -ENOENT;
    default:
        return -EIO;
    }

    if (strncmp(xipath.witness->path, xipath.path, len) != 0) {
        return -ENOENT;
    }

    if ((size = xipfs_file_get_size_(xipath.witness)) < 0) {
        return -EIO;
    }

    (void)memset(buf, 0, sizeof(*buf));
    buf->st_dev = (dev_t)(uintptr_t)mp;
    buf->st_ino = (ino_t)(uintptr_t)xipath.witness;
    if (path[len-1] != '/') {
        buf->st_mode = S_IFREG;
    } else {
        buf->st_mode = S_IFDIR;
    }
    buf->st_nlink = 1;
    buf->st_size = size;
    buf->st_blksize = XIPFS_NVM_PAGE_SIZE;
    buf->st_blocks = xipath.witness->reserved / XIPFS_NVM_PAGE_SIZE;

    return 0;
}

int
xipfs_statvfs(xipfs_mount_t *mp, const char *restrict path,
              struct xipfs_statvfs *restrict buf)
{
    unsigned free_pages, page_number;
    int ret;

    UNUSED(path);

    if ((ret = xipfs_mp_check(mp)) < 0) {
        return ret;
    }
    if (buf == NULL) {
        return -EFAULT;
    }

    if ((ret = xipfs_fs_get_page_number(mp)) < 0) {
        return -EIO;
    }
    page_number = (unsigned)ret;

    if ((ret = xipfs_fs_free_pages(mp)) < 0) {
        return -EIO;
    }
    free_pages = (unsigned)ret;

    (void)memset(buf, 0, sizeof(*buf));
    buf->f_bsize = XIPFS_NVM_PAGE_SIZE;
    buf->f_frsize = XIPFS_NVM_PAGE_SIZE;
    buf->f_blocks = page_number;
    buf->f_bfree = free_pages;
    buf->f_bavail = free_pages;
    buf->f_flag = ST_NOSUID;
    buf->f_namemax = XIPFS_PATH_MAX;

    return 0;
}

/*
 * xipfs-specific functions
 */

int
xipfs_new_file(xipfs_mount_t *mp, const char *path,
               uint32_t size, uint32_t exec)
{
    xipfs_path_t xipath;
    size_t len;
    int ret;

    if ((ret = xipfs_mp_check(mp)) < 0) {
        return ret;
    }
    if (path == NULL) {
        return -EFAULT;
    }
    if (path[0] == '\0') {
        return -ENOENT;
    }
    if (path[0] == '/' && path[1] == '\0') {
        return -EISDIR;
    }
    len = strnlen(path, XIPFS_PATH_MAX);
    if (len == XIPFS_PATH_MAX) {
        return -ENAMETOOLONG;
    }
    if (exec != 0 && exec != 1) {
        return -EINVAL;
    }

    if (xipfs_path_new(mp, &xipath, path) < 0) {
        return -EIO;
    }
    switch (xipath.info) {
    case XIPFS_PATH_EXISTS_AS_FILE:
        return -EEXIST;
    case XIPFS_PATH_EXISTS_AS_EMPTY_DIR:
    case XIPFS_PATH_EXISTS_AS_NONEMPTY_DIR:
        return -EISDIR;
    case XIPFS_PATH_INVALID_BECAUSE_NOT_DIRS:
        return -ENOTDIR;
    case XIPFS_PATH_INVALID_BECAUSE_NOT_FOUND:
        return -ENOENT;
    case XIPFS_PATH_CREATABLE:
        break;
    default:
        return -EIO;
    }

    if (xipath.path[xipath.len-1] == '/') {
        return -EISDIR;
    }
    if (xipath.witness != NULL && !(xipath.dirname[0] ==
            '/' && xipath.dirname[1] == '\0')) {
        if (strcmp(xipath.witness->path, xipath.dirname) == 0) {
            if (sync_remove_file(mp, xipath.witness) < 0) {
                return -EIO;
            }
        }
    }
    if (xipfs_fs_new_file(mp, path, size, exec) == NULL) {
        /* file creation failed */
        if (xipfs_errno == XIPFS_ENOSPACE ||
            xipfs_errno == XIPFS_EFULL) {
            return -EDQUOT;
        }
        return -EIO;
    }

    return 0;
}

static int
xipfs_execv_check(xipfs_mount_t *mp, const char *path,
                  char *const argv[],
                  const void *user_syscalls[XIPFS_USER_SYSCALL_MAX],
                  xipfs_path_t *xipath) {
    size_t len;
    int ret;

    if ((ret = xipfs_mp_check(mp)) < 0) {
        return ret;
    }
    if (path == NULL) {
        return -EFAULT;
    }
    if (path[0] == '\0') {
        return -ENOENT;
    }
    if (path[0] == '/' && path[1] == '\0') {
        return -EISDIR;
    }
    len = strnlen(path, XIPFS_PATH_MAX);
    if (len == XIPFS_PATH_MAX) {
        return -ENAMETOOLONG;
    }
    if (argv == NULL) {
        return -EFAULT;
    }
    if(user_syscalls == NULL)
        return -EFAULT;
    for(int i = 0; i < XIPFS_USER_SYSCALL_MAX; ++i) {
        if (user_syscalls[i] == NULL) {
            return -EFAULT;
        }
    }

    if (xipfs_path_new(mp, &xipath, path) < 0) {
        return -EIO;
    }
    switch (xipath.info) {
    case XIPFS_PATH_EXISTS_AS_FILE:
        break;
    case XIPFS_PATH_EXISTS_AS_EMPTY_DIR:
    case XIPFS_PATH_EXISTS_AS_NONEMPTY_DIR:
        return -EISDIR;
    case XIPFS_PATH_INVALID_BECAUSE_NOT_DIRS:
        return -ENOTDIR;
    case XIPFS_PATH_INVALID_BECAUSE_NOT_FOUND:
    case XIPFS_PATH_CREATABLE:
        return -ENOENT;
    default:
        return -EIO;
    }

    switch (xipath.witness->exec) {
    case 0:
        return -EACCES;
    case 1:
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

int
xipfs_execv(xipfs_mount_t *mp, const char *path,
            char *const argv[],
            const void *user_syscalls[XIPFS_USER_SYSCALL_MAX])
{
    xipfs_path_t xipath;
    int ret;

    ret = xipfs_execv_check(mp, path, argv, user_syscalls, &xipath);
    if (ret < 0)
        return ret;

    if ((ret = xipfs_file_exec(xipath.witness, argv, user_syscalls)) < 0) {
        return -EIO;
    }

    return ret;
}

int
xipfs_safe_execv(xipfs_mount_t *mp, const char *path,
            char *const argv[],
            const void *user_syscalls[XIPFS_USER_SYSCALL_MAX])
{
    xipfs_path_t xipath;
    int ret;

    ret = xipfs_execv_check(mp, path, argv, user_syscalls, &xipath);
    if (ret < 0)
        return ret;

    if ((ret = xipfs_file_safe_exec(xipath.witness, argv, user_syscalls)) < 0) {
        return -EIO;
    }

    return ret;
}
