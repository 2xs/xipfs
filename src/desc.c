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

#include <assert.h>
#include <errno.h>

#include "include/file.h"
#include "include/xipfs.h"

/**
 * @brief The descriptor type
 */
typedef enum desc_type_e {
    DESC_FREE,
    DESC_FILE,
    DESC_DIR,
} desc_type_t;

/**
 * @brief A descriptor entry
 */
typedef struct desc_entry_s {
    desc_type_t type;
    void *addr;
} desc_entry_t;

/**
 * @internal
 *
 * @brief An internal table that references the pointers to the
 * data structures of open descriptors
 */
static desc_entry_t _open_desc[XIPFS_MAX_OPEN_DESC];

/**
 * @internal
 *
 * @pre descp must be a pointer that references an accessible
 * memory region
 *
 * @brief Keeps track of a newly opened descriptor structure
 *
 * @param descp A pointer to a memory region containing the
 * descriptor structure to keep track
 *
 * @return Returns zero if the function succeeds or a negative
 * value otherwise
 */
static int
_xipfs_desc_track(void *descp, desc_type_t type)
{
    size_t entry = 0, i;
    int found = 0;

    assert(descp != NULL);
    assert(type != DESC_FREE);

    for (i = 0; i < XIPFS_MAX_OPEN_DESC; i++) {
        if (_open_desc[i].addr == NULL) {
            if (found == 0) {
                /* empty entry */
                entry = i;
                found = 1;
            }
        }
        if (_open_desc[i].addr == descp) {
            /* already tracked */
            return -EIO;
        }
    }
    if (found == 0) {
        /* no more entry */
        return -ENFILE;
    }

    _open_desc[entry].addr = descp;
    _open_desc[entry].type = type;

    return 0;
}

/**
 * @pre descp must be a pointer that references an accessible
 * memory region
 *
 * @brief Keeps track of a newly opened file descriptor
 * structure
 *
 * @param descp A pointer to a memory region containing the
 * file descriptor structure to keep track
 *
 * @return Returns zero if the function succeeds or a negative
 * value otherwise
 */
int
xipfs_file_desc_track(xipfs_file_desc_t *descp)
{
    if (descp == NULL) {
        return -EFAULT;
    }

    return _xipfs_desc_track(descp, DESC_FILE);
}

/**
 * @pre descp must be a pointer that references an accessible
 * memory region
 *
 * @brief Keeps track of a newly opened directory descriptor
 * structure
 *
 * @param descp A pointer to a memory region containing the
 * directory descriptor structure to keep track
 *
 * @return Returns zero if the function succeeds or a negative
 * value otherwise
 */
int
xipfs_dir_desc_track(xipfs_dir_desc_t *descp)
{
    if (descp == NULL) {
        return -EFAULT;
    }

    return _xipfs_desc_track(descp, DESC_DIR);
}

/**
 * @internal
 *
 * @pre descp must be a pointer that references an accessible
 * memory region
 *
 * @brief Stop keeping track of an open descriptor structure
 *
 * @param descp A pointer to a memory region containing the
 * descriptor structure to stop keeping track
 *
 * @return Returns zero if the function succeeds or a negative
 * value otherwise
 */
static int
_xipfs_desc_untrack(void *descp, desc_type_t type)
{
    size_t entry, i;
    int found = 0;

    assert(descp != NULL);
    assert(type != DESC_FREE);

    for (i = 0; i < XIPFS_MAX_OPEN_DESC; i++) {
        if (_open_desc[i].addr == descp) {
            if (found == 1) {
                /* tracked twice */
                return -EIO;
            }
            if (_open_desc[i].type != type) {
                /* not expected type */
                return -EIO;
            }
            entry = i;
            found = 1;
        }
    }
    if (found == 0) {
        /* not found */
        return -EIO;
    }

    _open_desc[entry].addr = NULL;
    _open_desc[entry].type = DESC_FREE;

    return 0;
}

/**
 * @pre descp must be a pointer that references an accessible
 * memory region
 *
 * @brief Stop keeping track of an open file descriptor
 * structure
 *
 * @param descp A pointer to a memory region containing the
 * file descriptor structure to stop keeping track
 *
 * @return Returns zero if the function succeeds or a negative
 * value otherwise
 */
int
xipfs_file_desc_untrack(xipfs_file_desc_t *descp)
{
    if (descp == NULL) {
        return -EFAULT;
    }

    return _xipfs_desc_untrack(descp, DESC_FILE);
}

/**
 * @pre descp must be a pointer that references an accessible
 * memory region
 *
 * @brief Stop keeping track of an open directory descriptor
 * structure
 *
 * @param descp A pointer to a memory region containing the
 * directory descriptor structure to stop keeping track
 *
 * @return Returns zero if the function succeeds or a negative
 * value otherwise
 */
int
xipfs_dir_desc_untrack(xipfs_dir_desc_t *descp)
{
    if (descp == NULL) {
        return -EFAULT;
    }

    return _xipfs_desc_untrack(descp, DESC_DIR);
}

/**
 * @internal
 *
 * @pre descp must be a pointer that references an accessible
 * memory region
 *
 * @brief Check whether an open descriptor structure is tracked
 *
 * @param descp A pointer to a memory region containing the
 * descriptor structure to check
 *
 * @return Returns zero if the descriptor structure is tracked or
 * a negative value otherwise
 */
static int
_xipfs_desc_tracked(void *descp, desc_type_t type)
{
    int found = 0;
    size_t i;

    for (i = 0; i < XIPFS_MAX_OPEN_DESC; i++) {
        if (_open_desc[i].addr == descp) {
            if (found == 1) {
                /* tracked twice */
                return -EIO;
            }
            if (_open_desc[i].type != type) {
                /* not expected type */
                return -EIO;
            }
            found = 1;
        }
    }

    return (found == 1) ? 0 : -EBADF;
}

/**
 * @pre descp must be a pointer that references an accessible
 * memory region
 *
 * @brief Check whether an open file descriptor structure is
 * tracked
 *
 * @param descp A pointer to a memory region containing the
 * file descriptor structure to check
 *
 * @return Returns zero if the file descriptor structure is
 * tracked or a negative value otherwise
 */
int
xipfs_file_desc_tracked(xipfs_file_desc_t *descp)
{
    return _xipfs_desc_tracked(descp, DESC_FILE);
}

/**
 * @pre descp must be a pointer that references an accessible
 * memory region
 *
 * @brief Check whether an open directory descriptor structure
 * is tracked
 *
 * @param descp A pointer to a memory region containing the
 * directory descriptor structure to check
 *
 * @return Returns zero if the directory descriptor structure is
 * tracked or a negative value otherwise
 */
int
xipfs_dir_desc_tracked(xipfs_dir_desc_t *descp)
{
    return _xipfs_desc_tracked(descp, DESC_DIR);
}

/**
 * @pre mp must be a pointer that references a memory region
 * containing an xipfs mount point structure which is accessible
 * and valid
 *
 * @brief Stop keeping track of all open descriptor structures
 * for the mount point specified as an argument
 *
 * @param mp A pointer to a memory region containing an xipfs
 * mount point structure
 */
int
xipfs_desc_untrack_all(xipfs_mount_t *mp)
{
    xipfs_file_desc_t *file_descp;
    xipfs_dir_desc_t *dir_descp;
    uintptr_t filp, start, end;
    size_t i;

    if (mp == NULL) {
        return -EFAULT;
    }

    start = (uintptr_t)mp->page_addr;
    end = start + mp->page_num * XIPFS_NVM_PAGE_SIZE;
    for (i = 0; i < XIPFS_MAX_OPEN_DESC; i++) {
        switch (_open_desc[i].type) {
        case DESC_FILE:
            file_descp = _open_desc[i].addr;
            filp = (uintptr_t)file_descp->filp;
            if (filp != (uintptr_t)xipfs_infos_file) {
                if (filp >= start  && filp < end) {
                    _open_desc[i].addr = NULL;
                    _open_desc[i].type = DESC_FREE;
                }
            }
            break;
        case DESC_DIR:
            dir_descp = _open_desc[i].addr;
            filp = (uintptr_t)dir_descp->filp;
            if (filp != (uintptr_t)xipfs_infos_file) {
                if (filp >= start  && filp < end) {
                    _open_desc[i].addr = NULL;
                    _open_desc[i].type = DESC_FREE;
                }
            }
            break;
        case DESC_FREE:
        default:
            break;
        }
    }

    return 0;
}

/**
 * @pre mp must be a pointer that references a memory region
 * containing an xipfs mount point structure which is accessible
 * and valid
 *
 * @pre removed must be a pointer that references an accessible
 * memory region
 *
 * @brief Update the tracked open descriptor structures by
 * modifying the internal address of the xipfs file, following
 * the removal of a file at the mount point, with both elements
 * provided as arguments
 *
 * @param mp A pointer to a memory region containing an xipfs
 * mount point structure
 *
 * @param removed A pointer to a memory region containing the
 * removed xipfs file structure
 *
 * @param reserved The reserved size of the removed xipfs file
 * structure
 */
int
xipfs_desc_update(xipfs_mount_t *mp, xipfs_file_t *removed,
                  size_t reserved)
{
    xipfs_file_desc_t *file_descp;
    xipfs_dir_desc_t *dir_descp;
    uintptr_t filp, start, end;
    size_t i;

    if (mp == NULL) {
        return -EFAULT;
    }
    if (removed == NULL) {
        return -EFAULT;
    }

    start = (uintptr_t)mp->page_addr;
    end = start + mp->page_num * XIPFS_NVM_PAGE_SIZE;
    for (i = 0; i < XIPFS_MAX_OPEN_DESC; i++) {
        switch (_open_desc[i].type) {
        case DESC_FILE:
            file_descp = _open_desc[i].addr;
            filp = (uintptr_t)file_descp->filp;
            if (filp != (uintptr_t)xipfs_infos_file) {
                if (filp >= start  && filp < end) {
                    if (filp > (uintptr_t)removed) {
                        file_descp->filp = (xipfs_file_t *)
                            (uintptr_t)file_descp->filp - reserved;
                    } else if (filp == (uintptr_t)removed) {
                        _open_desc[i].addr = NULL;
                        _open_desc[i].type = DESC_FREE;
                    }
                }
            }
            break;
        case DESC_DIR:
            dir_descp = _open_desc[i].addr;
            filp = (uintptr_t)dir_descp->filp;
            if (filp != (uintptr_t)xipfs_infos_file) {
                if (filp >= start  && filp < end) {
                    if (filp > (uintptr_t)removed) {
                        dir_descp->filp = (xipfs_file_t *)
                            (uintptr_t)dir_descp->filp - reserved;
                    } else if (filp == (uintptr_t)removed) {
                        _open_desc[i].addr = NULL;
                        _open_desc[i].type = DESC_FREE;
                    }
                }
            }
            break;
        case DESC_FREE:
        default:
            continue;
        }
    }

    return 0;
}
