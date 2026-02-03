/*******************************************************************************/
/*  © Université de Lille, The Pip Development Team (2015-2025)                */
/*  Copyright (C) 2020-2025 Orange                                             */
/*                                                                             */
/*  This software is a computer program whose purpose is to run a filesystem   */
/*  with in-place execution and memory isolation.                              */
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
#include <stddef.h>
#include <string.h>

/*
 * xipfs includes
 */
#include "include/xipfs.h"
#include "include/buffer.h"
#include "include/errno.h"
#include "include/file.h"
#include "include/flash.h"
#include "include/fs.h"

/*
 * Macro definition
 */

/**
 * @internal
 *
 * @def ROUND
 *
 * @brief Round x to the next power of two y
 *
 * @param x The number to round to the next power of two y
 *
 * @param y The power of two with which to round x
 */
#define ROUND(x, y) (((x) + (y) - 1) & ~((y) - 1))

/*
 * Extern functions
 */

/**
 * @pre xipfs_mp must be a pointer that references a memory
 * region containing an xipfs mount point structure which is
 * accessible and valid
 *
 * @brief Retrieves the first xipfs file in the mount point's
 * linked list passed as an argument
 *
 * @param xipfs_mp A pointer to a memory region containing an
 * xipfs mount point structure
 *
 * @return Returns the address of the first xipfs file in the
 * mount point's linked list or NULL otherwise
 */
xipfs_file_t *
xipfs_fs_head(xipfs_mount_t *mp)
{
    xipfs_file_t *headp;

    headp = mp->page_addr;
    if ((int)headp->next == (int)XIPFS_FLASH_ERASE_STATE) {
        /* no file in the file system */
        return NULL;
    }
    if (xipfs_file_filp_check(headp) < 0) {
        /* xipfs_errno was set */
        return NULL;
    }

    return headp;
}

/**
 * @pre filp must be a pointer that references an accessible
 * memory region
 *
 * @brief Retrieves the next xipfs file of the linked list from
 * the xipfs file structure passed as an argument
 *
 * @param xipfs_mp A pointer to a memory region containing an
 * accessible xipfs file structure
 *
 * @return Returns the address of the next xipfs file of the
 * linked list from the xipfs file structure passed as an
 * argument or NULL otherwise
 */
xipfs_file_t *
xipfs_fs_next(xipfs_file_t *filp)
{
    xipfs_file_t *nextp;

    if (xipfs_file_filp_check(filp) < 0) {
        /* xipfs_errno was set */
        return NULL;
    }

    if (filp->next == filp) {
        /* no more files - file system full */
        return NULL;
    }

    nextp = filp->next;

    if ((int)nextp->next == (int)XIPFS_FLASH_ERASE_STATE) {
        /* no more files - file system not full */
        return NULL;
    }

    if (xipfs_file_filp_check(nextp) < 0) {
        /* xipfs_errno was set */
        return NULL;
    }

    return nextp;
}

/**
 * @pre xipfs_mp must be a pointer that references a memory
 * region containing an xipfs mount point structure which is
 * accessible and valid
 *
 * @brief Retrieves the last xipfs file in the mount point's
 * linked list passed as an argument
 *
 * @param xipfs_mp A pointer to a memory region containing an
 * xipfs mount point structure
 *
 * @return Returns the address of the last xipfs file in the
 * mount point's linked list or NULL otherwise
 */
xipfs_file_t *
xipfs_fs_tail(xipfs_mount_t *mp)
{
    xipfs_file_t *filp, *tailp;

    if ((filp = xipfs_fs_head(mp)) == NULL) {
        /* no file in the file system or error */
        return NULL;
    }
    do {
        tailp = filp;
        xipfs_errno = XIPFS_OK;
        filp = xipfs_fs_next(filp);
    } while (filp != NULL);

    if (xipfs_errno != XIPFS_OK) {
        /* xipfs_errno was set */
        return NULL;
    }

    return tailp;
}

/**
 * @pre xipfs_mp must be a pointer that references a memory
 * region containing an xipfs mount point structure which is
 * accessible and valid
 *
 * @brief Retrieves the address of the first free NVM page in
 * the mount point passed as an argument
 *
 * @param xipfs_mp A pointer to a memory region containing an
 * xipfs mount point structure
 *
 * @return Returns the address of the first free NVM page in the
 * mount point or NULL otherwise
 */
xipfs_file_t *
xipfs_fs_tail_next(xipfs_mount_t *mp)
{
    xipfs_file_t *tailp;

    xipfs_errno = XIPFS_OK;
    if ((tailp = xipfs_fs_tail(mp)) == NULL) {
        if (xipfs_errno != XIPFS_OK) {
            /* xipfs_errno was set */
            return NULL;
        }
        /* no file in the file system */
        return mp->page_addr;
    }
    if (tailp->next == tailp) {
        xipfs_errno = XIPFS_EFULL;
        return NULL;
    }

    return tailp->next;
}

/**
 * @pre xipfs_mp must be a pointer that references a memory
 * region containing an xipfs mount point structure which is
 * accessible and valid
 *
 * @brief Retrieves the number of NVM page in the mount point
 * passed as an argument
 *
 * @param xipfs_mp A pointer to a memory region containing an
 * xipfs mount point structure
 *
 * @return Returns the number of NVM page in the mount point
 * address or a negative value otherwise
 */
int
xipfs_fs_get_page_number(xipfs_mount_t *mp)
{
    return mp->page_num;
}

/**
 * @pre xipfs_mp must be a pointer that references a memory
 * region containing an xipfs mount point structure which is
 * accessible and valid
 *
 * @brief Retrieves the number of NVM free page in the mount
 * point passed as an argument
 *
 * @param xipfs_mp A pointer to a memory region containing an
 * xipfs mount point structure
 *
 * @return Returns the number of NVM free page in the mount
 * point address or a negative value otherwise
 */
int
xipfs_fs_free_pages(xipfs_mount_t *mp)
{
    xipfs_file_t *headp, *tailp;
    int used, free;

    xipfs_errno = XIPFS_OK;
    if ((headp = xipfs_fs_head(mp)) == NULL) {
        if (xipfs_errno != XIPFS_OK) {
            /* xipfs_errno was set */
            return -1;
        }
        /* all pages are free */
        return mp->page_num;
    }
    if ((tailp = xipfs_fs_tail(mp)) == NULL) {
        /* xipfs_errno was set */
        return -1;
    }
    used  = (unsigned)tailp;
    used += (unsigned)tailp->reserved;
    used -= (unsigned)headp;
    used /= XIPFS_NVM_PAGE_SIZE;
    free = mp->page_num - used;

    return free;
}

/**
 * @pre xipfs_mp must be a pointer that references a memory
 * region containing an xipfs mount point structure which is
 * accessible and valid
 *
 * @pre path must be a pointer that references a path which is
 * accessible, null-terminated, starts with a slash, normalized,
 * and shorter than XIPFS_PATH_MAX
 *
 * @brief Creates a new file in the file system specified by the
 * mount point structure passed as an argument
 *
 * @param xipfs_mp A pointer to a memory region containing an
 * xipfs mount point structure
 *
 * @param path A pointer to a path
 *
 * @param size Determines how many pages of NVM will be reserved
 * for the file
 *
 * @return Returns a pointer to the newly created xipfs file
 * structure or NULL otherwise
 */
xipfs_file_t *
xipfs_fs_new_file(xipfs_mount_t *mp, const char *path, size_t size,
                  int exec)
{
    int free_pages, reserved_pages;
    xipfs_file_t file, *filp;
    size_t reserved;
    void *next;

    if (xipfs_file_path_check(path) < 0) {
        /* xipfs_errno was set */
        return NULL;
    }
    if (exec != 0 && exec != 1) {
        xipfs_errno = XIPFS_EPERM;
        return NULL;
    }
    if ((filp = xipfs_fs_tail_next(mp)) == NULL) {
        /* xipfs_errno was set */
        return NULL;
    }
    if ((free_pages = xipfs_fs_free_pages(mp)) < 0) {
        /* xipfs_errno was set */
        return NULL;
    }

    if (size > 0) {
        reserved = ROUND(size + sizeof(xipfs_file_t), XIPFS_NVM_PAGE_SIZE);
    } else {
        reserved = XIPFS_NVM_PAGE_SIZE;
    }
    reserved_pages = reserved / XIPFS_NVM_PAGE_SIZE;

    if (reserved_pages < free_pages) {
        next = (char *)filp + reserved;
    } else if (reserved_pages == free_pages) {
        next = filp;
    } else {
        xipfs_errno = XIPFS_ENOSPACE;
        return NULL;
    }

    (void)memset(&file, XIPFS_NVM_ERASE_STATE, sizeof(file));
    (void)strncpy(file.path, path, XIPFS_PATH_MAX - 1);
    file.reserved = reserved;
    file.next = next;
    file.exec = exec;

    if (xipfs_buffer_write(filp, &file, sizeof(*filp)) < 0) {
        /* xipfs_errno was set */
        return NULL;
    }
    if (xipfs_buffer_flush() < 0) {
        /* xipfs_errno was set */
        return NULL;
    }

    return filp;
}

/**
 * @pre dst must be a pointer that references an accessible
 * memory region
 *
 * @brief Removes a file from the file system and consolidates
 * it
 *
 * @param dst The address of the xipfs file to remove
 *
 * @return Returns zero if the function succeeds or a negative
 * value otherwise
 */
int
xipfs_fs_remove(void *dst)
{
    xipfs_file_t file;
    size_t pagenum, i;
    void *src, *next;
    unsigned int num;
    uint32_t size;

    assert(dst != NULL);

    xipfs_errno = XIPFS_OK;
    /* get the next file if any */
    if ((next = xipfs_fs_next(dst)) == NULL) {
        if (xipfs_errno != XIPFS_OK) {
            /* xipfs_errno was set */
            return -1;
        }
    }
    /* erase the file's pages */
    if (xipfs_file_erase(dst) < 0) {
        /* xipfs_errno was set */
        return -1;
    }
    /*
     * Consolidate the file system by moving files
     * after the deleted one.
     */
    while (next != NULL) {
        src = next;
        if ((next = xipfs_fs_next(src)) == NULL) {
            if (xipfs_errno != XIPFS_OK) {
                /* xipfs_errno was set */
                return -1;
            }
        }
        /* copy and fix up the file structure */
        (void)memcpy(&file, src, sizeof(file));
        size = (uintptr_t)((xipfs_file_t *)src)->next - (uintptr_t)src;
        file.next = (xipfs_file_t *)((uintptr_t)dst + size);
        if (xipfs_flash_write_unaligned(dst, &file, sizeof(file)) < 0) {
            /* xipfs_errno was set */
            return -1;
        }
        /* copy the rest of the file's first page */
        if (xipfs_flash_write_unaligned(
            (char *)dst + sizeof(file),
            (char *)src + sizeof(file),
            XIPFS_NVM_PAGE_SIZE - sizeof(file)) < 0) {
            /* xipfs_errno was set */
            return -1;
        }
        if (xipfs_flash_erase_page(xipfs_nvm_page(src)) < 0) {
            /* xipfs_errno was set */
            return -1;
        }
        dst = (char *)dst + XIPFS_NVM_PAGE_SIZE;
        src = (char *)src + XIPFS_NVM_PAGE_SIZE;
        /* first page of the file already copied */
        pagenum = size / XIPFS_NVM_PAGE_SIZE;
        for (i = 1; i < pagenum; i++) {
            num = xipfs_nvm_page(src);
            if (xipfs_flash_is_erased_page(num) == 0) {
                if (xipfs_flash_write_unaligned(dst, src,
                        XIPFS_NVM_PAGE_SIZE) < 0) {
                    /* xipfs_errno was set */
                    return -1;
                }
                if (xipfs_flash_erase_page(num) < 0) {
                    /* xipfs_errno was set */
                    return -1;
                }
            }
            dst = (char *)dst + XIPFS_NVM_PAGE_SIZE;
            src = (char *)src + XIPFS_NVM_PAGE_SIZE;
        }
    }

    return 0;
}

/**
 * @pre xipfs_mp must be a pointer that references a memory
 * region containing an xipfs mount point structure which is
 * accessible and valid
 *
 * @brief Format the file system specified by the mount point
 * structure passed as an argument
 *
 * @param xipfs_mp A pointer to a memory region containing an
 * xipfs mount point structure
 *
 * @return Returns zero if the function succeeds or a negative
 * value otherwise
 */
int
xipfs_fs_format(xipfs_mount_t *mp)
{
    unsigned start_page, end_page;
    void *start_addr, *end_addr;
    size_t i;

    start_addr = mp->page_addr;
    end_addr = (char *)start_addr + mp->page_num * XIPFS_NVM_PAGE_SIZE;

    start_page = xipfs_nvm_page(start_addr);
    end_page = xipfs_nvm_page(end_addr);

    i = 0;
    while ((start_page + i) < end_page) {
        if (xipfs_flash_erase_page(start_page + i) < 0) {
            /* xipfs_errno was set */
            return -1;
        }
        i++;
    }

    return 0;
}

/**
 * @pre xipfs_mp must be a pointer that references a memory
 * region containing an xipfs mount point structure which is
 * accessible and valid
 *
 * @pre from must be a pointer that references a path which is
 * accessible, null-terminated, starts with a slash, normalized,
 * and shorter than XIPFS_PATH_MAX
 *
 * @pre to must be a pointer that references a path which is
 * accessible, null-terminated, starts with a slash, normalized,
 * and shorter than XIPFS_PATH_MAX
 *
 * @brief Renames all xipfs file starting with the from prefix
 * to the to prefix in the mount point structure passed as an
 * argument
 *
 * @param xipfs_mp A pointer to a memory region containing an
 * xipfs mount point structure
 *
 * @param from Renames all paths with this prefix
 *
 * @param to Renames all paths with the prefix from to the to
 * prefix
 *
 * @return Returns zero if the function succeeds or a negative
 * value otherwise
 */
int
xipfs_fs_rename_all(xipfs_mount_t *mp, const char *from, const char *to)
{
    char path[XIPFS_PATH_MAX];
    size_t from_len, to_len;
    xipfs_file_t *filp;
    int counter;

    from_len = strnlen(from, XIPFS_PATH_MAX);
    if (from_len == XIPFS_PATH_MAX) {
        xipfs_errno = XIPFS_ENULTER;
        return -1;
    }
    to_len = strnlen(to, XIPFS_PATH_MAX);
    if (to_len == XIPFS_PATH_MAX) {
        xipfs_errno = XIPFS_ENULTER;
        return -1;
    }

    (void)strcpy(path, to);
    xipfs_errno = XIPFS_OK;
    counter = 0;
    if ((filp = xipfs_fs_head(mp)) != NULL) {
        do {
            if (strncmp(filp->path, from, from_len) == 0) {
                /* XXX Handle file name truncation */
                (void)strncpy(&path[to_len], &filp->path[from_len],
                    XIPFS_PATH_MAX-to_len);
                path[XIPFS_PATH_MAX-1] = '\0';
                if (xipfs_file_rename(filp, path) < 0) {
                    /* xipfs_errno was set */
                    return -1;
                }
                counter++;
            }
        } while ((filp = xipfs_fs_next(filp)) != NULL);
    }
    if (xipfs_errno != XIPFS_OK) {
        /* xipfs_errno was set */
        return -1;
    }

    return counter;
}
